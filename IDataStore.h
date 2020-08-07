#ifndef IDATASTORE_H
#define IDATASTORE_H

#include "Config.h"

#include <vector>
#include <map>


//testing
#include <thread>
#include <chrono>
#include <mutex>


#define CONNECTION_THREAD_SLEEP				60		// how long cleaner sleeps between runs
#define CONNECTION_THREAD_MAGIC				2		// consecutives cycles of a met condition for conditions to be cleaned

#define DAY_MILLI								86400000
#define DAY_SEC								86400
#define DAY_MIN								1440


struct Account {
	char *account_name;
	char *account_uuid;
	char *user_name;
	char *password;
	char *old_password;
	char *url;
	long long update_time;
	bool deleted;
	
	Account() = default;
	Account(char *account_name, char *uuid, char *user_name, char *pass, char *old_pass, char *url, long long update, bool deleted) :
	account_name{account_name},
	account_uuid{uuid},
	user_name{user_name},
	password{pass},
	old_password{old_pass},
	url{url},
	update_time{update},
	deleted{deleted}
	{}
};


struct Heap_List {

	char *heap;
	Heap_List *next;
	
	Heap_List() : next{nullptr} 
	{
		heap = NULL;
	};
	
	Heap_List(unsigned int sz)
	{
		heap = (char*)malloc(sz);
		next = nullptr;
	}
	
	~Heap_List() {
		if (heap != NULL) {
			free(heap);
		}
		
		delete next;
	};
};



class IDataStore
{
public:
//	virtual IDataStore() {}
	virtual ~IDataStore() {}
	
	virtual bool initialize(Config *) = 0;
	
	virtual std::vector<User> getAllUsers() = 0;
	virtual bool createUser(User &user) = 0;
	virtual bool delete_user(User &user) = 0;
	virtual std::map<char *, Account, cmp_key> get_accounts_for_user(const char *user, Heap_List * heap) = 0;
	virtual bool upsert_accounts_for_user(const char *user, std::vector<Account> &accounts) = 0;
	virtual bool update_last_sync_for_user(const char *user, long long lockTime) = 0;
	

protected:
	
	int currentConnections;
	std::vector<void *> connections;
	// cv is needed since a client will wait for a connection if all are used and pool is at max size already
	std::condition_variable connections_cv;
	std::mutex connections_mutex;
	Config *config;
	
	// overrider to close connections from connection manager thread
	virtual void closeConn(void*) {}
	// override to delete user accounts whose based off of current time compared to last sync
	virtual void deleteOldUsers(long time_ms) {}
	// override to delete history table of accounts
	virtual void deleteHistory(long time_sec) {}
	
	
	inline Heap_List * increase_list_buffer(unsigned int needed, unsigned int &sz, unsigned int &indx, Heap_List * heap) 
	{
		if (needed + 20 > sz - indx) {
//printf("\nHEAP_LIST REALLOC NEEDED\n");
			Heap_List *new_heap = new Heap_List{1024};
			heap->next = new_heap;
			sz = 1024;
			indx = 0;
			return new_heap;
		} else {
//printf("\nHEAP_LIST REALLOC NOT NEEDED\n");
			return heap;
		}
	}
	
	
	
	
	/*
	 * If called by implementing class cleans db connections no longer needed during idl(er) times
	 */
	virtual void start_connection_manager(Config *conf)
	{
		printf("Starting db connection manager thread\n");
		config = conf;

		std::thread manager([&] () {
			int minConn = (config->getStoreType() == redis_store) ? 
							config->getRedisMinConnections() : config->getDbMinConnections();
			int counter{0};
			int magic_number{CONNECTION_THREAD_MAGIC};
			std::vector<void *> conns_to_close;

			while (true) {
				std::this_thread::sleep_for(std::chrono::seconds(CONNECTION_THREAD_SLEEP));
				
				connections_mutex.lock();
				int curr = currentConnections;
				int in_use = curr - connections.size();
				
				if (curr < minConn) {
					//need to add connections, let the sync handler threads do it
				} else if (curr == minConn || in_use > 0.2 * curr) {
					//nothing to do, at min or enough cons are being used so dont shrink
					counter = 0;
				} else {
					//increment counter and check if connections need to be removed
					if (++counter >= magic_number) {
						int to_close = (curr - minConn) * 0.4;
						
						if (to_close < 1)
							to_close = curr - minConn;
							
						for (size_t i{0}; i<to_close; i++) {
							conns_to_close.push_back(connections.back());
							connections.pop_back();
							currentConnections--;
						}
					}
				}
				
				connections_mutex.unlock();
				connections_cv.notify_one();
				printf("Data Store connection current connections: %d, inuse: %d, counter: %d\n", curr, minConn, counter);
				
				if (conns_to_close.size() > 0) {
					printf("Closing %lu data store connections\n", conns_to_close.size());
					
					for (void *conn : conns_to_close)
						closeConn(conn);
						
					conns_to_close.clear();
					counter = 0;
				}
			}
		});
		
		manager.detach();
	}



	long get_next_run()
	{
		time_t h = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		tm *local = localtime(&h);
		long sleep_for_minutes = 59 - local->tm_min;
		sleep_for_minutes += (23 - local->tm_hour) * 60;
		return sleep_for_minutes + 2;
	}


	/* 
	 * If called deletes user/accounts whose last sync is older then specified in config.
	 * interval is in days, how often the cleaner runs, at most once a day
	 * days is how long to keep deleted accounts in the history table
	 * 
	 */
	virtual void start_data_cleaner(Config *conf)
	{
		std::thread cleaner([&] () {
			int interval =  (config->getStoreType() == redis_store) ? 
							  config->getRedisCleanerInterval() : config->getDbCleanerInterval();
			int days = (config->getStoreType() == redis_store) ?
						 config->getRedisCleanerPurgeDays() : config->getDbCleanerPurgeDays();
			int history_days = (config->getStoreType() == redis_store) ?
								 config->getRedisCleanerHistoryPurgeDays() : config->getDbCleanerHistoryPurgeDays();

			//first run will be at next midnight + 2 minutes no matter what the interval is
			long next_run = get_next_run();
			printf("DB Cleaner started, interval: %d, history days: %d, days: %d, next run: %ld minutes\n", interval, history_days, days, next_run);
			
			while (true) {
				printf("DB Cleaner sleeping for %ld minutes\n", next_run);
				std::this_thread::sleep_for(std::chrono::minutes(next_run));
				printf("DB Cleaner starting task\n");
				
				long curr_sec = 
					std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				long history_delete_sec = curr_sec - (history_days * DAY_SEC);
				deleteOldUsers(history_delete_sec);
				deleteHistory(history_delete_sec);
				
				next_run = get_next_run() + (DAY_MIN * (1-interval));
				printf("Next run will in %ld minutes\n", next_run);
			}
		});
		
		cleaner.detach();
	}

};


#include "data_store_connection.h"
#include "RedisStore.h"


#endif  //IDATASTORE_H
