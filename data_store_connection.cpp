#include "data_store_connection.h"

#include <string>
#include <thread>
#include <chrono>
#include <ctime>


float account_format = 1.00;

data_store_connection::data_store_connection() //: connected{false}
{
}

data_store_connection::~data_store_connection()
{
//	PQfinish(conn);
	connections_mutex.lock();
	
	for (PGconn *conn : connections)
		PQfinish(conn);
		
	connections.clear();
	connections_mutex.unlock();
	printf("data_store_connection destroyed\n");
}


bool data_store_connection::initialize(Config *config) 
{
	this->config = config;
	current_connections = config->getDbMinConnections();
	max_connections = config->getDbMaxConnections();
	
	for (size_t i{0}; i < config->getDbMinConnections(); i++) {

		try {
			PGconn *conn = connect();

			if (createPreparedStatements(conn)) {
				connections.push_back(conn);
			} else {
				current_connections--;
				PQfinish(conn);
			}
		} catch (const char *error) {
			printf("Error in data_store initialize: %s\n", error);
			current_connections--;
		}
	}

	printf("Data store initialized with: %d:%d connections\n", current_connections, connections.size());
	start_connection_manager();
	
	if (config->isDbCleaner())
		start_data_cleaner();
	
	return (current_connections > 0) ? true : false;
}


bool data_store_connection::createPreparedStatements(PGconn* conn)
{
	bool success{true};
	
	if (!prepare(conn, create_user_pre, CREATE_USER_PREPARE, 4))
		success = false;
	
	if (!prepare(conn, delete_user_pre, DELETE_USER_PREPARE, 1))
		success = false;
	
	if (!prepare(conn, update_last_sync_time_pre, UPDATE_USER_LAST_SYNC_PREPARE, 2))
		success = false;
	
	if (!prepare(conn, get_accounts_for_user_pre, GET_ACCOUNTS_FOR_USER_PREPARE, 1))
		success = false;
	
//	if (!prepare(conn, upsert_account_pre, UPSERT_ACCOUNT_PREPARE, 8))
//		success = false;
	
	//should be 4 statements for this connection
	PGresult *res = PQexec(conn, "select * from pg_prepared_statements");
	
	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		printf("Check prepare count failed: %s\n", PQerrorMessage(conn));
	} else {
		printf("Prepared %d statements\n", PQntuples(res));
	}

	PQclear(res);
	return success;
}


bool data_store_connection::prepare(PGconn* conn, const char *prep_name, const std::string &prep_def, int count)
{
	bool success{true};
	PGresult *res = PQprepare(conn, prep_name, prep_def.c_str(), count, NULL);
						
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("PQprepare %s, failed: %s\n", prep_name, PQerrorMessage(conn));
		success = false;
	}
	
	PQclear(res);
	return success;
}


PGconn* data_store_connection::connect()
{
	bool connected{false};
	int attempts{1};
	std::string conn_info{""};
	conn_info.append("hostaddr=").append(config->getDbServer());
	conn_info.append(" port=").append(config->getDbPort());
	conn_info.append(" dbname=").append(config->getDbName());
	conn_info.append(" user=").append(config->getDbUsername());
	conn_info.append(" password=").append(config->getDbPassword());
	int interval{1};
	PGconn *conn;
	
	do {
		conn = PQconnectdb(conn_info.c_str());

		if (PQstatus(conn) != CONNECTION_OK) {
			fprintf(stderr, "Attempt %d, Connection to database failed: %s", attempts, PQerrorMessage(conn));
			PQfinish(conn);
			//std::this_thread::sleep_for(std::chrono::milliseconds(CONNECT_ATTEMPTS_INTERVAL * 1000));
			std::this_thread::sleep_for(std::chrono::milliseconds(interval++ * 1000));
			connected = false;
		} else {
			connected = true;
		}
		
	} while (!connected && attempts++ < CONNECT_ATTEMPTS);
	
	if (connected)
		return conn;
	else
		throw "Unable to connect to database";
}


std::vector<User> data_store_connection::getAllUsers()
{
	std::vector<User> users;
	
	try {
		PGresult *res = PQexec_wrapper(GET_USERS_SQL.c_str());
			
		for (int j = 0; j < PQntuples(res); j++) {
			users.emplace(users.begin(), std::string{PQgetvalue(res, j, 0)},
			std::string{PQgetvalue(res, j, 1)}, atoll(PQgetvalue(res, j, 2)));
		}
				
		PQclear(res);
	} catch (const char *error) {
		printf("Error getting all userers, %s\n", error);
	}
	
	return users;
}


std::map<std::string, Account> data_store_connection::get_accounts_for_user(std::string& user)
{
	std::map<std::string, Account> accounts;
	const char *values[1];
	values[0] = user.c_str();
	
	try {
		PGresult *res = PQexecPrepared_wrapper(get_accounts_for_user_pre, values, 1);
	
		for (int i = 0; i < PQntuples(res); i++) {
			accounts.emplace(std::string{PQgetvalue(res, i, 0)}, Account{std::string{PQgetvalue(res, i, 0)}, 
				std::string{PQgetvalue(res, i, 1)},
				std::string{PQgetvalue(res, i, 2)}, std::string{PQgetvalue(res, i, 3)},
				std::string{PQgetvalue(res, i, 4)}, std::string{PQgetvalue(res, i, 5)},
				atoll(PQgetvalue(res, i, 6)), 
				((std::string{PQgetvalue(res, i, 7)}=="t") ? true : false)}); //TODO TODO revist bool postgres c lib
		}
		
		PQclear(res);
	} catch (const char* error) {
		printf("Error getting accounts for user: %s, error: %s\n", user.c_str(), error);
		throw "Error in ::get_accounts_for_user";
	}
			
	return accounts;
}


bool data_store_connection::upsert_accounts_for_user(std::string& user, std::vector<Account>& accounts)
{
	if (accounts.size() < 1) 
		return true;
	
	bool success{true};
	const char *values[8 * accounts.size()];
	std::string sql = UPSERT_ACCOUNT_PREPARE_1;
	int i{0};

	for (Account &a : accounts) {
		if (i++ != 0)
			sql += ", ";
		sql += "('" + a.account_name + "', '" + user + "', '" + a.user_name + "', '" + a.password + "', '" +
			a.old_password + "', '" + a.url + "', " + std::to_string(a.update_time) + ", " + 
			((a.deleted) ? "true" : "false") + ")";
	}
	
	sql += " " + UPSERT_ACCOUNT_PREPARE_2;
	
	try {
		PGresult *res = PQexec_wrapper(sql.c_str());
		PQclear(res);
	} catch (const char* error) {
		printf("Error upserting for user: %s, error: %s\n", user.c_str(), error);
		success = false;
		throw "Error in ::upsert_accounts_for_user";
	}

	return success;
}


bool data_store_connection::createUser(User& user)
{
	bool success{true};
	const char *values[4];
	values[0] = user.account_uuid.c_str();
	values[1] = user.account_password.c_str();
	values[2] = std::to_string(user.account_last_sync).c_str();
//	values[3] = std::to_string(account_format).c_str();
	values[3] = "1.00";
	
	try {
		PGresult *res = PQexecPrepared_wrapper(create_user_pre, values, 4);
		PQclear(res);
	} catch (const char* error) {
		printf("Error creating user: %s, error: %s\n", user.account_uuid.c_str(), error);
		success = false;
		throw "Error in ::createUser";
	}
	
	return success;
}


bool data_store_connection::update_last_sync_for_user(std::string& user, long long lockTime)
{
	bool success{true};
	const char *values[2];
	values[0] = std::to_string(lockTime).c_str();
	values[1] = user.c_str();
	
	try {
		PGresult *res = PQexecPrepared_wrapper(update_last_sync_time_pre, values, 2);
		PQclear(res);
	} catch (const char* error) {
		printf("Error updating last sync for user: %s, error: %s\n", user.c_str(), error);
		success = false;
		throw "Error in ::update_last_sync_for_user";
	}
	
	return success;
}


bool data_store_connection::delete_user(User& user)
{
	bool success{true};
	const char *values[1];
	values[0] = user.account_uuid.c_str();
	
	try {
		PGresult *res = PQexecPrepared_wrapper(delete_user_pre, values, 1);
		PQclear(res);
	} catch (const char* error) {
		printf("Error deleting user: %s, error: %s\n", user.account_uuid.c_str(), error);
		success = false;
		throw "Error in ::delete_user";
	}
	
	return success;
}


PGresult* data_store_connection::PQexecPrepared_wrapper(const char* pre, const char *values[], int values_cnt)
{
	bool success = true;
//	PGconn *conn = get_connection();
	PGconn *conn;
	PGresult *res;
	
	for (size_t i{0}; i < 3; i++) {
		try {
			conn = get_connection();
		} catch (const char *error) {
			printf("Error getting connection in ::PQexecPrepared_wrapper, error: %s\n", error);
			throw "Error executing PQexec_wrapper";
		}

		res = PQexecPrepared(conn, pre, values_cnt, values, NULL, NULL, 0);
		
		if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_TUPLES_OK) {
			printf("PQExec %s, failed: %s\n", pre, PQerrorMessage(conn));
			success = false;
			PQclear(res);
			
			if (PQstatus(conn) != CONNECTION_OK) {
				printf("DB connection bad\n");
				connections_mutex.lock();
				current_connections--;
				connections_mutex.unlock();
				connections_cv.notify_one();
				PQfinish(conn);
			}
			//reset_connection(conn);
		} else {
			release_connection(conn);
			success = true;
			break;
		}
	}
	
	if (success)
		return res;
	else
		throw "Error executing PQexecPrepared_wrapper";
}


PGresult* data_store_connection::PQexec_wrapper(const char* sql)
{
	bool success = true;
	PGconn *conn;
	PGresult *res;
	
	for (size_t i{0}; i < 3; i++) {
		try {
			conn = get_connection();
		} catch (const char *error) {
			printf("Error getting connection in ::PQexec_wrapper, error: %s\n", error);
			throw "Error executing PQexec_wrapper";
		}

		res = PQexec(conn, sql);
		
		if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_TUPLES_OK) {
			printf("PQExec %s, failed: %s\n", sql, PQerrorMessage(conn));
			success = false;
			PQclear(res);
				
			if (PQstatus(conn) != CONNECTION_OK) {
				printf("DB connection bad\n");
				connections_mutex.lock();
				current_connections--;
				connections_mutex.unlock();
				connections_cv.notify_one();
				PQfinish(conn);
			}
		} else {
			success = true;
			release_connection(conn);
			break;
		}
	}
	
	if (success)
		return res;
	else
		throw "Error executing PQexec_wrapper";
}


void data_store_connection::release_connection(PGconn* conn)
{
	std::unique_lock<std::mutex> lock{connections_mutex};
	connections.push_back(conn);
	lock.unlock();
	connections_cv.notify_one();
}


PGconn* data_store_connection::get_connection()
{
	PGconn *conn;
	printf("unused db connections: %d, current connections count: %d\n", connections.size(), current_connections);
	std::unique_lock<std::mutex> lock{connections_mutex};
	
	if (connections.size() > 0) {
		//get a connection
		conn = connections.back();
		connections.pop_back();
	} else if (current_connections < max_connections) {
		//create a new connection
		current_connections++;
		lock.unlock();
		printf("Creating new db connection\n");
		
		try {
			conn = connect();
		
			if (createPreparedStatements(conn)) {
				return conn;
			} else {
				PQfinish(conn);
				lock.lock();
				current_connections--;
				lock.unlock();
				connections_cv.notify_one();
				throw "Error creating prepared statemnets in ::get_connection";
			}
		} catch (const char *error) {
			lock.lock();
			current_connections--;
			lock.unlock();
			connections_cv.notify_one();
			throw "Error getting connection in ::get_connection";
		}
	} else {
		//all connections used and max created so wait
		connections_cv.wait(lock, [&] {
			printf("Waiting for db connection to becom free\n");
			return connections.size() > 0;
		});
		
		conn = connections.back();
		connections.pop_back();
	}
	
	connections_cv.notify_one();
	return conn;
}


void data_store_connection::start_connection_manager()
{
	printf("Starting db connection manager thread\n");
	
	std::thread manager([&] () {
		int min_conn = config->getDbMinConnections();
		int counter{0};
		int magic_number{2};
		std::vector<PGconn *> conns_to_close;
		
		while (true) {
			std::this_thread::sleep_for(std::chrono::seconds(60));
//			printf("Data Store connection manager is woke\n");
			
			connections_mutex.lock();
			int curr = current_connections;
			int in_use = curr - connections.size();
//			connections_mutex.unlock();
//			connections_cv.notify_one();
			
			if (curr < min_conn) {
				//need to add connections, let the sync handler threads do it
			} else if (curr == min_conn || in_use > 0.2 * curr) {
				//nothing to do, at min or enough cons are being used so dont shrink
				counter = 0;
			} else {
				//increment counter and check if connections need to be removed
				if (++counter >= magic_number) {
					int to_close = (curr - min_conn) * 0.4;
					
					if (to_close < 1)
						to_close = curr - min_conn;
						
					for (size_t i{0}; i<to_close; i++) {
						conns_to_close.push_back(connections.back());
						connections.pop_back();
						current_connections--;
					}
				}
			}
			
			connections_mutex.unlock();
			connections_cv.notify_one();
//			printf("Data Store connection current connections: %d, inuse: %d, counter: %d\n", curr, in_use, counter);
			
			if (conns_to_close.size() > 0) {
				printf("Closing %d data store connections\n", conns_to_close.size());
				
				for (PGconn *conn : conns_to_close)
					PQfinish(conn);
					
				conns_to_close.clear();
				counter = 0;
			}
		}
	});
	
	manager.detach();
}


long current_time_sec()
{
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

long current_time_milli()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

long get_next_run()
{
	time_t h = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	tm *local = localtime(&h);
	long sleep_for_minutes = 59 - local->tm_min;
	sleep_for_minutes += (23 - local->tm_hour) * 60;
	return sleep_for_minutes + 2;
}

// interval is in days, how often the cleaner runs, at most once a day
// days is how long to keep deleted accounts in the history table
void data_store_connection::start_data_cleaner()
{
	std::thread cleaner([&] () {
		int interval = config->getDbCleanerInterval();
		int days = config->getDbCleanerPurgeDays();
		int history_days = config->getDbCleanerHistoryPurgeDays();
		long day_in_milli{86400000};
		long day_in_sec{86400};
		long day_in_min{1440};
		//first run will be at next midnight + 2 minutes no matter what the interval is
		long next_run = get_next_run();
		printf("DB Cleaner started, interval: %d, history days: %d, days: %d, next run: %d minutes\n", interval, history_days, days, next_run);
		
		while (true) {
			printf("DB Cleaner sleeping for %ld minutes\n", next_run);
			std::this_thread::sleep_for(std::chrono::minutes(next_run));
			printf("DB Cleaner starting task\n");
			
			long curr_sec = current_time_sec();
			long history_delete_sec = curr_sec - (history_days * day_in_sec);
			printf("DB cleaner removing any history accounts older then %ld\n", history_delete_sec);
			std::string select_sql_smt = ACCOUNT_HISTORY_SQL + std::to_string(history_delete_sec);
			std::string delete_sql_smt = DELETE_HISTORY_SQL + std::to_string(history_delete_sec);
			
			long curr_milli = current_time_milli();
			long delete_milli = curr_milli - (days * day_in_milli);
			printf("DB cleaner removing any current accounts older then %ld\n", delete_milli);
			std::string select_old_sql_smt = OLD_ACCOUNT_SQL + std::to_string(delete_milli);
			std::string delete_old_sql_smt = DELETE_OLD_ACCOUNTS_SQL + std::to_string(delete_milli);
			
			try {
				PGresult *res = PQexec_wrapper(select_old_sql_smt.c_str());
				int cnt = PQntuples(res);
			
				if (cnt > 0) {
					printf("DB cleaner, the following current accounts will be removed\n");
					
					for (size_t i{0}; i<cnt; i++) {
						printf("user: %s, account: %s\n", PQgetvalue(res, i, 0), PQgetvalue(res, i, 1));
					}
					
					PQclear(res);
					res = PQexec_wrapper(delete_old_sql_smt.c_str());
					PQclear(res);
				} else {
					printf("DB cleaner, no current accounts will be removed\n");
					PQclear(res);
				}
				
			} catch (const char *error) {
				printf("DB cleaner, error executing current accounts delete: %s\n", error);
			}
			
			try {
				PGresult *res = PQexec_wrapper(select_sql_smt.c_str());
				int cnt = PQntuples(res);
			
				if (cnt > 0) {
					printf("DB cleaner, the following history accounts will be removed\n");
					
					for (size_t i{0}; i<cnt; i++) {
						printf("user: %s, account: %s\n", PQgetvalue(res, i, 0), PQgetvalue(res, i, 1));
					}
					
					PQclear(res);
					res = PQexec_wrapper(delete_sql_smt.c_str());
					PQclear(res);
				} else {
					printf("DB cleaner, no history accounts will be removed\n");
					PQclear(res);
				}
				
			} catch (const char *error) {
				printf("DB cleaner, error executing history delete: %s\n", error);
			}
			
			next_run = get_next_run() + (day_in_min * (1-interval));
			printf("Next run will in %d minutes\n", next_run);
		}
	});
	
	cleaner.detach();
}
