#include "RedisStore.h"

#include <thread>
#include <chrono>




RedisStore::RedisStore()
{
	printf("RedisStore created, their words not mine, oO0OoO0OoO0Oo Redis is starting oO0OoO0OoO0Oo\n");

}

RedisStore::~RedisStore()
{
}


bool RedisStore::initialize(Config* config)
{
	this->config = config;
	connections.reserve(config->getRedisMinConnections());
	currentConnections = 0;
	maxConnections = config->getRedisMaxConnections();
	
	for (size_t i=0; i<config->getRedisMinConnections(); i++) {
		redisContext *c = createConnection();
		
		if (c != NULL) {
			connections.push_back(c);
			currentConnections++;
		}
	}
	
	if (connections.size() > 0) {
		start_connection_manager(config);
		
		if (config->isRedisCleaner())
			start_data_cleaner(config);
		
		return true;
	} else {
		return false;
	}
	
}


std::vector<User> RedisStore::getAllUsers() {
	return getAllUsers(true);
}

/*
 * This is called once at startup by a single sync_handler thread. All other threads will block
 * until this calls is success. No reason to use attempt counts, instead try until success with 1 sec sleep
 */
std::vector<User> RedisStore::getAllUsers(bool tryForever)
{
	std::vector<User> users;
	redisContext *c;
	redisReply *reply, *replySync;
	long lastSync;
	
	int attempt = 1;					// counter before giving up
	bool success = false;				// flag when command is success
	bool testConn = false;				// never test conn on first attempt assume it is good
	
	do {
		c = NULL;
		
		try {
			c = getConnection(testConn);
			reply = (redisReply*) redisCommand(c, REDIS_CMD_GETUSERS);
			
			if (!areReplyErrors(reply, c)) {
				// No errors
				success = true;
				
				if (reply->type == REDIS_REPLY_ARRAY) {
					printf("\n\nRedisStore Loading %zu users...\n\n", reply->elements/2);
					
					for (size_t i=0; i<reply->elements; i+=2) {
						//get last sync for user
						replySync = (redisReply*) redisCommand(c, REDIS_CMD_GETUSERSYNC, reply->element[i]->str);
						
						if (areReplyErrors(replySync, c)) {
							success = false;
//							releaseConnection(c);
							break;
						} else {
							lastSync = (long) atol(replySync->str);
							freeReplyObject(replySync);
							
							if (lastSync < 1)
								lastSync = current_time_ms();
						}
						
						printf("\tUser: %s - %ld added\n", reply->element[i]->str, lastSync);
						users.emplace(users.begin(), reply->element[i]->str, reply->element[i+1]->str, lastSync);
					}
					
//					releaseConnection(c);
					
				} else {
					printf("ERROR RedisStore::getAllUsers() expected Array reply, but got %d\n", reply->type);
					freeReplyObject(reply);
//					releaseConnection(c);
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				}
			} else {
				// ERROR - release connection and try again if not max attempts
//				releaseConnection(c);
				testConn = true;
			}
			
		} catch (const char* error) {
			printf("ERROR: RedisStore::getAllUsers() - %s\n", error);
//			releaseConnection(c);
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			testConn = true;
		}
		
		releaseConnection(c);
		 
	} while (!success && (attempt++ <= REDIS_MAX_ATTEMPS || tryForever));
	
	if (!success) {
		printf("\n\n!!! ERROR RedisStore::getAllUsers() failed to load users !!!\n\n");		//should never happen
	}
	
	return users;
}


bool RedisStore::createUser(User& user)
{
	redisContext *c;
//	redisReply *reply;
	
	int attempt = 1;					// counter before giving up
	bool success{false};				// flag when command is success
	bool testConn{false};				// never test conn on first attempt assume it is good
	
	do {
		success = true;
		c = NULL;		//set to NULL each loop, if getConnection fails and throws, dont try to release some Addr, segfault
		
		try {
			c = getConnection(testConn);
			testConn = true;
			
//			long long syncTime = current_time_ms();
			redisAppendCommand(c, REDIS_CMD_CREATEUSER, user.account_uuid, user.account_password);
			redisAppendCommand(c, REDIS_CMD_SETLASTSYNC, user.account_uuid, user.account_last_sync);
			redisAppendCommand(c, REDIS_CMD_DELACCOUNTLIST, user.account_uuid);

			success = eatPipelineReplies(3, c);
			
			releaseConnection(c);

		} catch (const char *error) {
			releaseConnection(c);
			success = false;
		}
	} while (!success && attempt++ < REDIS_MAX_ATTEMPS);
	
	return success;
}


bool RedisStore::delete_user(User& user)
{
	redisContext *c;
	redisReply *reply;
	
	int attempt = 1;					// counter before giving up
	bool success{true};				// flag when command is success
	bool testConn{false};				// never test conn on first attempt assume it is good
	
	do {
		c = NULL;
		success = true;
		
		try {
			c = getConnection(testConn);
			testConn = true;
			reply = (redisReply*)redisCommand(c, REDIS_CMD_GETACCOUNTS, user.account_uuid);
			
			if (!areReplyErrors(reply, c)) {	
				if (reply->type == REDIS_REPLY_ARRAY) {
				
					for (size_t i=0; i < reply->elements; i++) {
//						printf("-> %s\n", reply->element[i]->str);
						redisAppendCommand(c, REDIS_CMD_DELACCOUNT, user.account_uuid, reply->element[i]->str);
					}
					
					success = eatPipelineReplies(reply->elements, c);
					
					freeReplyObject(reply);
					
					if (success) {
						redisAppendCommand(c, REDIS_CMD_DELACCOUNTLIST, user.account_uuid);
						redisAppendCommand(c, REDIS_CMD_DELUSER, user.account_uuid);
						redisAppendCommand(c, REDIS_CMD_DELUSERSYNC, user.account_uuid);
						success = eatPipelineReplies(3, c);
					}
				} else {
					printf("\n\nRedisStore::delete_user did not return array, returned: %d\n\n", reply->type);
					success = false;
				}
			} else {
				//error
				success = false;
			}
			
			releaseConnection(c);
		
		} catch (const char *error) {
			success = false;
			releaseConnection(c);
		}
		
	} while (!success && attempt++ <= REDIS_MAX_ATTEMPS);
	
	return success;
}


std::map<char*,Account,cmp_key> RedisStore::get_accounts_for_user(const char* user, Heap_List* heap_head)
{
	std::map<char*, Account, cmp_key> accounts;
	
	// used for temp heap
	unsigned int indx = 0;
	unsigned int sz;
	Heap_List *cur_heap = heap_head;
	char *heap;
	
	unsigned int len[5];
	char *val[5];

	bool deleted;
	long updateTime;

	redisContext *c;
	redisReply *reply;
	
	int attempt = 1;					// counter before giving up
	bool success{true};				// flag when command is success
	bool testConn{false};				// never test conn on first attempt assume it is good
	
	size_t cnt{0};
	
	do {
		c = NULL;
		success = true;
		
		try {
			c = getConnection(testConn);
			testConn = true;
			reply = (redisReply*)redisCommand(c, REDIS_CMD_GETACCOUNTS, user);
			
			if (!areReplyErrors(reply, c)) {	
				if (reply->type == REDIS_REPLY_ARRAY) {
					cnt = reply->elements;
					sz = cnt * 256;
					heap_head->heap = (char*)malloc(sz);

					for (size_t i=0; i < cnt; i++) {
//						printf("-> %s\n", reply->element[i]->str);
						redisAppendCommand(c, REDIS_CMD_GETACCOUNT, user, reply->element[i]->str);
					}
					
					freeReplyObject(reply);
					
					for (size_t i=0; i < cnt; i++) {
						if (redisGetReply(c,(void **)&reply) == REDIS_OK) {
							
							if (reply->elements != ACCOUNT_MEMBERS) {
								// accounts hset has key for account that no longer exists. Would be exhausitive
								// to get all the keys again and search for each separately so leave
								// shouldnt really ever happen
								continue;
							}
						
							for (size_t j=0; j < ACCOUNT_MEMBERS; j+=2) {
								if (reply->element[j]->str[0] == 'a') {	//account name
									val[0] = reply->element[j+1]->str;
									len[0] = strlen(val[0]);
								} else if (reply->element[j]->str[0] == 'p') {		//password
									val[1] = reply->element[j+1]->str;
									len[1] = strlen(val[1]);
								} else if (reply->element[j]->str[0] == 'o') {		//old pass
									val[2] = reply->element[j+1]->str;
									len[2] = strlen(val[2]);
								} else if (reply->element[j]->str[0] == 'd') {		//deleted
									deleted = (reply->element[j+1]->str[0] == '1') ? true : false;
								} else {
									if (reply->element[j]->str[1] == 's') {		//user
										val[3] = reply->element[j+1]->str;
										len[3] = strlen(val[3]);
									} else if (reply->element[j]->str[1] == 'r') {		//url
										val[4] = reply->element[j+1]->str;
										len[4] = strlen(val[4]);
									} else if (reply->element[j]->str[1] == 'p') {		//update
										updateTime = atol((char*)reply->element[j+1]->str);
									}
								}
							}

							cur_heap = increase_list_buffer(len[0]+len[1]+len[2]+len[3]+len[4], sz, indx, cur_heap);
							heap = cur_heap->heap;
										
							for (unsigned int j=0; j<5; j++) {
								strcpy(&heap[indx], val[j]);
								indx += len[j] + 1;
								len[j] = indx - len[j] - 1;
							}

							accounts.emplace(&heap[len[0]], Account{&heap[len[0]], (char*)user, &heap[len[3]], &heap[len[1]],
										&heap[len[2]], &heap[len[4]], updateTime, deleted});

							freeReplyObject(reply);
						} else {
							if (! areReplyErrors(reply, c)) {
								freeReplyObject(reply);
							}
											
							success = false;
							printf("\nRedisStore::get_accounts_for_user - ERROR getting Account, user: %s !!!\n\n", user);
							break;
						}
					}
					
				} else {
					printf("\n\nRedisStore::get_accounts_for_user did not return array, returned: %d\n\n", reply->type);
					freeReplyObject(reply);
					success = false;
				}
				
			} else {
				success = false;
			}
			
		} catch (const char *error) {
			success = false;
			printf("\n\nRedisStore::get_accounts_for_user error: %s\n", error);
		}
		
		releaseConnection(c);
		
	} while (!success && attempt++ <= REDIS_MAX_ATTEMPS);
	
	return accounts;
}


bool RedisStore::update_last_sync_for_user(const char* user, long long lastSync)
{
	redisContext *c;
	
	int attempt = 1;					// counter before giving up
	bool success{true};				// flag when command is success
	bool testConn{false};				// never test conn on first attempt assume it is good
	
	do {
		c = NULL;
		success = true;
		
		try {
			c = getConnection(testConn);
			testConn = true;
			redisAppendCommand(c, REDIS_CMD_SETLASTSYNC, user, (long)lastSync);
			success = eatPipelineReplies(1, c);
		} catch (const char *error) {
			printf("\n\nRedisStore::update_last_sync_for_user Error!!!\n\n");
		}
		
		releaseConnection(c);
	
	} while (!success && attempt++ <= REDIS_MAX_ATTEMPS);
	
	return success;
}


bool RedisStore::upsert_accounts_for_user(const char* user, std::vector<Account>& accounts)
{
	redisContext *c;
	
	int attempt = 1;					// counter before giving up
	bool success{true};				// flag when command is success
	bool testConn{false};				// never test conn on first attempt assume it is good
	
	do {
		c = NULL;
		success = true;
		
		try {
			c = getConnection(testConn);
			testConn = true;
			
			for (Account &account : accounts) {
				redisAppendCommand(c, REDIS_CMD_ADDTOACCOUNTS, user, account.account_name);
				redisAppendCommand(c, REDIS_CMD_SETACCOUNT, user, account.account_name, account.account_name, 
						 account.user_name,  account.password,  account.old_password,  account.url,
						  account.update_time, ((account.deleted) ? 1 : 0));
			}
			
			success = eatPipelineReplies(accounts.size() * 2, c);
	
		} catch (const char *error) {
			printf("RedisStore::upsert_accounts_for_user ERROR - %s\n", error);
		}
	
		releaseConnection(c);
		
	} while (!success && attempt++ <= REDIS_MAX_ATTEMPS);
	
	return success;
}


redisContext* RedisStore::createConnection()
{
//TODO - loop through x number of time to retry on failure
	redisContext *c = redisConnect(config->getRedisServer().c_str(), config->getRedisPort());

	if (c == NULL || c->err) {
		if (c) {
			printf("Error creating Redis context: %s\n", c->errstr);
			redisFree(c);
			c = NULL;
		} else {
			printf("Can't allocate redis context\n");
		}
	} else {
		redisReply *reply = (redisReply*)redisCommand(c, REDIS_CMD_AUTH, 
					config->getRedisUsername().c_str(), config->getRedisPassword().c_str());

		if (areReplyErrors(reply, c)) {
			redisFree(c);
			c = NULL;
		} else {
			printf("Redis Connection Created/Authenticated: %d - %s\n", reply->type, reply->str);
			freeReplyObject(reply);
		}
	}
	
	return c;
}


redisContext* RedisStore::getConnection(bool testConn)
{
	redisContext *c;
	printf("Unused Redis db connections: %lu, current connections count: %d\n", connections.size(), currentConnections);
	
	// get lock
	std::unique_lock<std::mutex> lock{connections_mutex};
	
	if (connections.size() > 0) {
		// connection available get one
		c = (redisContext*) connections.back();
		connections.pop_back();
		
		lock.unlock();
		connections_cv.notify_one();
		
		if (testConn) {
			c = testConnection(c);
		}
	} else if (currentConnections < maxConnections) {
		// no connections available, but pool not at max so create one
		c = createConnection();
		
		if (c != NULL) {
			currentConnections++;
			lock.unlock();
			connections_cv.notify_one();
		} else {
			lock.unlock();
			connections_cv.notify_one();
			throw "Unable to create new Redis Connection";
		}
	} else {
		// no connections available, pool is at max, wait for a connection to be returned
		connections_cv.wait(lock, [&] {
			printf("Waiting for Redis db connection\n");
			return connections.size() > 0;
		});
		
		c = (redisContext*) connections.back();
		connections.pop_back();
		lock.unlock();
		connections_cv.notify_one();
		
		if (testConn) {
			c = testConnection(c);
		}
	}
	
//	lock.unlock();
//	connections_cv.notify_one();
	return c;
}


redisContext *RedisStore::testConnection(redisContext *c)
{
	bool good = false;
			
	while (!good) {
		redisReply *reply = (redisReply*)redisCommand(c, REDIS_CMD_TEST);
				
		if (!areReplyErrors(reply, c)) {
			freeReplyObject(reply);
			good = true;
		} else {
			releaseConnection(c);
			c = getConnection(true);
		}
	}
	
	return c;
}


void RedisStore::releaseConnection(redisContext *c)
{
	connections_mutex.lock();
	
	if (c != NULL) {
		if (c->err == REDIS_OK) {
			//add connection back, still good
			connections.push_back(c);
		} else {
			//connection is now bad dont add back
			fprintf(stderr, "\n\n    CONNECTION error: %d - %s\n\n", c->err, c->errstr);
			redisFree(c);
			currentConnections--;
		}
	} else {
		currentConnections--;
	}
	
	connections_mutex.unlock();
	connections_cv.notify_one();
}


bool RedisStore::areReplyErrors(redisReply *reply, redisContext *c)
{
	if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
		if (reply) {
			printf("Redis error: %s\n", reply->str);
			freeReplyObject(reply);
		} else {
			printf("Redis error: %d - %s\n", c->err, c->errstr);
		}
				
		return true;
	} else {
		return false;
	}
	
}


bool RedisStore::eatPipelineReplies(size_t cnt, redisContext *c)
{
	redisReply *reply = NULL;
	bool success{true};
	
	for (size_t i=0; i < cnt; i++) {
		if (redisGetReply(c,(void **)&reply) == REDIS_OK) {
			freeReplyObject(reply);
		} else {
			if (! areReplyErrors(reply, c)) {
				freeReplyObject(reply);
			}
							
			success = false;
			printf("\nRedisStore::eatPipelineReplies - ERROR getting a reply!!!\n\n");
			break;
		}
	}
	
	return success;
}


/*
 * Call back for connection manager thread to close connections
 */
void RedisStore::closeConn(void *conn)
{
	redisFree((redisContext*)conn);
}


/*
 * Call back for db cleaner thread
 */
void RedisStore::deleteOldUsers(long time_ms)
{
	redisContext *c;
	
	int attempt = 1;					// counter before giving up
	bool success = true;				// flag when command is success
	bool testConn = false;				// never test conn on first attempt assume it is good

	do {
		c = NULL;
		success = true;
		
		try {
			std::vector<User> users = getAllUsers(false);
			c = getConnection(testConn);
			testConn = true;
		
			for ( User user : users) {

				if (user.account_last_sync < time_ms) {
					// delete users accounts
					Heap_List heap;
					std::map<char*,Account,cmp_key> accounts = get_accounts_for_user(user.account_uuid, &heap);
					
					for (auto iter = accounts.begin(); iter != accounts.end(); iter++) {
						redisAppendCommand(c, REDIS_CMD_DELACCOUNT, user.account_uuid, iter->second.account_name);
						printf("RedisStore::deleteOldUsers - deleting account: %s.%s\n", user.account_uuid, iter->second.account_name);
					}
					
					if (eatPipelineReplies(accounts.size(), c)) {
						redisAppendCommand(c, REDIS_CMD_DELACCOUNTLIST, user.account_uuid);
						success = eatPipelineReplies(1, c);
					} else {
						fprintf(stderr, "\n\nERROR - RedisStore::deleteOldUsers error: err deleting account for user: %s\n\n", user.account_uuid);
						success = false;
					}
				}
				
				if (!success) {
					fprintf(stderr, "\n\nERROR - RedisStore::deleteOldUsers error: err deleting accountsList for: %s\n\n", user.account_uuid);
					break;
				}
			}
		
		} catch (const char *error) {
			fprintf(stderr, "\n\nERROR - RedisStore::deleteOldUsers error: %s\n\n", error);
			testConn = true;
			success = false;
		}
	
		releaseConnection(c);
		
	} while (!success && attempt++ <= REDIS_MAX_ATTEMPS);
	
	if (!success) {
		fprintf(stderr, "\n\nERROR - RedisStore::deleteOldUsers finished with errors..\n\n");
	}

}


/*
 * Call back for db cleaner thread
 */
void RedisStore::deleteHistory(long time_sec)
{
	printf("Redis Store does not currently keep account history accounts, returning...\n");
}

