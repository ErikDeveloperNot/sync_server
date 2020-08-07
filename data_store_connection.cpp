#include "data_store_connection.h"

#include <string>
#include <thread>
#include <chrono>
#include <ctime>
#include <cstring>


float account_format = 1.00;




data_store_connection::data_store_connection() //: connected{false}
{
}

data_store_connection::~data_store_connection()
{
//	PQfinish(conn);
	connections_mutex.lock();
	
	for (void *conn : connections)
		PQfinish((PGconn*) conn);
		
	connections.clear();
	connections_mutex.unlock();
	printf("data_store_connection destroyed\n");
}


bool data_store_connection::initialize(Config *config) 
{
	this->config = config;
	currentConnections = config->getDbMinConnections();
	max_connections = config->getDbMaxConnections();
	
	for (size_t i{0}; i < config->getDbMinConnections(); i++) {

		try {
			PGconn *conn = connect();

			if (createPreparedStatements(conn)) {
				connections.push_back(conn);
			} else {
				currentConnections--;
				PQfinish(conn);
			}
		} catch (const char *error) {
			printf("Error in data_store initialize: %s\n", error);
			currentConnections--;
		}
	}

	printf("Data store initialized with: %d:%lu connections\n", currentConnections, connections.size());
	start_connection_manager(config);
	
	if (config->isDbCleaner())
		start_data_cleaner(config);
	
	return (currentConnections > 0) ? true : false;
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


bool data_store_connection::prepare(PGconn* conn, const char *prep_name, const char *prep_def, int count)
{
	bool success{true};
	PGresult *res = PQprepare(conn, prep_name, prep_def, count, NULL);
						
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
//	conn_info.append("hostaddr=").append(config->getDbServer());			//todo - numeric version
	conn_info.append("host=").append(config->getDbServer());				//hostname version
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
		PGresult *res = PQexec_wrapper(GET_USERS_SQL);
		
		for (int j = 0; j < PQntuples(res); j++) {
			char *uuid = (char*)PQgetvalue(res, j, 0);
//			char *uuid = (char*)malloc(strlen(temp) + 1);
//			strcpy(uuid, temp);
			char *pass = (char*)PQgetvalue(res, j, 1);
//			char *pass= (char*)malloc(strlen(temp) + 1);
//			strcpy(pass, temp);
			users.emplace(users.begin(), uuid, pass, atol(PQgetvalue(res, j, 2)));
		}
				
		PQclear(res);
	} catch (const char *error) {
		printf("Error getting all userers, %s\n", error);
	}
	
	return users;
}


std::map<char *, Account, cmp_key> data_store_connection::get_accounts_for_user(const char *user, Heap_List * heap_head)
{
	std::map<char *, Account, cmp_key> accounts;
	const char *values[1];
	values[0] = user;
	unsigned int indx = 0;
	unsigned int sz;
	Heap_List *cur_heap = heap_head;
	unsigned int len[6];
	char *val[6];
	
	try {
		PGresult *res = PQexecPrepared_wrapper(get_accounts_for_user_pre, values, 1);
		sz = PQntuples(res) * 256;
		heap_head->heap = (char*)malloc(sz);
		char *heap;
	
		for (int i = 0; i < PQntuples(res); i++) {
			val[0] = PQgetvalue(res, i, 0);
			len[0] = strlen(val[0]);
			val[1] = PQgetvalue(res, i, 1);
			len[1] = strlen(val[1]);
			val[2] = PQgetvalue(res, i, 2);
			len[2] = strlen(val[2]);
			val[3] = PQgetvalue(res, i, 3);
			len[3] = strlen(val[3]);
			val[4] = PQgetvalue(res, i, 4);
			len[4] = strlen(val[4]);
			val[5] = PQgetvalue(res, i, 5);
			len[5] = strlen(val[5]);
			
			cur_heap = increase_list_buffer(len[0]+len[1]+len[2]+len[3]+len[4]+len[5], sz, indx, cur_heap);
			heap = cur_heap->heap;
			
			for (unsigned int j=0; j<6; j++) {
				strcpy(&heap[indx], val[j]);
				indx += len[j] + 1;
				len[j] = indx - len[j] - 1;
			}
		
			bool deleted = (PQgetvalue(res, i, 7)[0] == 't') ? true : false;
//printf("Account Name: %s, uuid: %s\n", &heap[len[0]], &heap[len[1]]);
			accounts.emplace(&heap[len[0]], Account{&heap[len[0]], &heap[len[1]], &heap[len[2]], &heap[len[3]],
				&heap[len[4]], &heap[len[5]], atol(PQgetvalue(res, i, 6)), deleted});
		}
		
		PQclear(res);
	} catch (const char* error) {
		printf("Error getting accounts for user: %s, error: %s\n", user, error);
		throw "Error in ::get_accounts_for_user";
	}
			
	return accounts;
}


bool data_store_connection::upsert_accounts_for_user(const char *user, std::vector<Account>& accounts)
{
	if (accounts.size() < 1) 
		return true;
	
	bool success{true};
//	const char *values[8 * accounts.size()];
	int i{0};
	unsigned int indx{0};
	unsigned int sz = 500 + (100 * accounts.size());
	char *buf = (char*) malloc(sz);
	unsigned int needed{0};
	indx = sprintf(buf, "%s", UPSERT_ACCOUNT_PREPARE_1);

	for (Account &a : accounts) {
		if (i++ != 0) {
			strcpy(&buf[indx], ", ");
			indx += 2;
		}
		
		needed = strlen(a.account_name) + strlen(a.user_name) + strlen(a.password) + strlen(a.old_password) +
			strlen(a.url) + 70;
			
		sz = increase_buffer(needed, sz, indx, buf);
		
		indx += sprintf(&buf[indx], "('%s', '%s', '%s', '%s', '%s', '%s', %ld, %s)", a.account_name, user,
			a.user_name, a.password, a.old_password, a.url, (long)a.update_time, (a.deleted) ? t : f);
	}
	
	sz = increase_buffer(300, sz, indx, buf);
	sprintf(&buf[indx], "%s", UPSERT_ACCOUNT_PREPARE_2);
//printf("\n\nSQL:\n%s\n\n", buf);
	
	try {
		PGresult *res = PQexec_wrapper(buf);
		PQclear(res);
	} catch (const char* error) {
		printf("Error upserting for user: %s, error: %s\n", user, error);
		success = false;
		free(buf);
		throw "Error in ::upsert_accounts_for_user";
	}

	free(buf);
	return success;
}


bool data_store_connection::createUser(User& user)
{
	bool success{true};
	const char *values[4];
	values[0] = user.account_uuid;
	values[1] = user.account_password;
	values[2] = std::to_string(user.account_last_sync).c_str();
	values[3] = "1.00";
	
	try {
		PGresult *res = PQexecPrepared_wrapper(create_user_pre, values, 4);
		PQclear(res);
	} catch (const char* error) {
		printf("Error creating user: %s, error: %s\n", user.account_uuid, error);
		success = false;
		throw "Error in ::createUser";
	}
	
	return success;
}


bool data_store_connection::update_last_sync_for_user(const char *user, long long lockTime)
{
//printf("UPDATE_LAST_SYNC_FOR_USER_1, user: %s, time: %lld\n", user.c_str(), lockTime);
	bool success{true};
	const char *values[2];
	sprintf(sync_buf, "%ld", (long)lockTime);
	values[0] = sync_buf;
	values[1] = user;
//printf("UPDATE_LAST_SYNC_FOR_USER_2, user: %s, time: %s\n", values[1], values[0]);
	
	try {
		PGresult *res = PQexecPrepared_wrapper(update_last_sync_time_pre, values, 2);
		PQclear(res);
	} catch (const char* error) {
		printf("Error updating last sync for user: %s, lockTime: %ld, error: %s\n", user, (long)lockTime, error);
		success = false;
		//throw "Error in ::update_last_sync_for_user";
	}
	
	return success;
//return true;
}


bool data_store_connection::delete_user(User& user)
{
	bool success{true};
	const char *values[1];
	values[0] = user.account_uuid;
	
	try {
		PGresult *res = PQexecPrepared_wrapper(delete_user_pre, values, 1);
		PQclear(res);
	} catch (const char* error) {
		printf("Error deleting user: %s, error: %s\n", user.account_uuid, error);
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
				currentConnections--;
				connections_mutex.unlock();
				connections_cv.notify_one();
				PQfinish(conn);
			}
else {
	release_connection(conn);
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
				currentConnections--;
				connections_mutex.unlock();
				connections_cv.notify_one();
				PQfinish(conn);
			}
else {
	release_connection(conn);
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
	printf("unused db connections: %lu, current connections count: %d\n", connections.size(), currentConnections);
	std::unique_lock<std::mutex> lock{connections_mutex};
	
	if (connections.size() > 0) {
		//get a connection
		conn = (PGconn*)connections.back();
		connections.pop_back();
	} else if (currentConnections < max_connections) {
		//create a new connection
		currentConnections++;
		lock.unlock();
		printf("Creating new db connection\n");
		
		try {
			conn = connect();
		
			if (createPreparedStatements(conn)) {
//				return conn;
			} else {
				PQfinish(conn);
				lock.lock();
				currentConnections--;
				lock.unlock();
				connections_cv.notify_one();
				throw "Error creating prepared statemnets in ::get_connection";
			}
		} catch (const char *error) {
			lock.lock();
			currentConnections--;
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
		
		conn = (PGconn*) connections.back();
		connections.pop_back();
	}
	
	connections_cv.notify_one();
	return conn;
}



/*
 * virtual implemnetation called by connection manager thread
 */
void data_store_connection::closeConn(void *conn)
{
	PQfinish((PGconn*)conn);
}


/*
 * virtual implemnetation called by db cleaner thread
 */
void data_store_connection::deleteOldUsers(long time_ms)
{
	std::string select_old_sql_smt = OLD_ACCOUNT_SQL + std::to_string(time_ms);
	std::string delete_old_sql_smt = DELETE_OLD_ACCOUNTS_SQL + std::to_string(time_ms);

	try {
		PGresult *res = PQexec_wrapper(select_old_sql_smt.c_str());
		int cnt = PQntuples(res);
				
		if (cnt > 0) {
			printf("Postgres DB cleaner, the following current accounts will be removed\n");
						
			for (size_t i{0}; i<cnt; i++) {
				printf("user: %s, account: %s\n", PQgetvalue(res, i, 0), PQgetvalue(res, i, 1));
			}
						
			PQclear(res);
			res = PQexec_wrapper(delete_old_sql_smt.c_str());
			PQclear(res);
		} else {
			printf("Postgres DB cleaner, no current accounts will be removed\n");
			PQclear(res);
		}
					
	} catch (const char *error) {
		printf("Postgres DB cleaner, error executing current accounts delete: %s\n", error);
	}
}


/*
 * virtual implemnetation called by db cleaner thread
 */
void data_store_connection::deleteHistory(long time_sec)
{
	std::string select_sql_smt = ACCOUNT_HISTORY_SQL + std::to_string(time_sec);
	std::string delete_sql_smt = DELETE_HISTORY_SQL + std::to_string(time_sec);
				
	try {
		PGresult *res = PQexec_wrapper(select_sql_smt.c_str());
		int cnt = PQntuples(res);
				
		if (cnt > 0) {
			printf("Postgres DB cleaner, the following history accounts will be removed\n");
						
			for (size_t i{0}; i<cnt; i++) {
				printf("user: %s, account: %s\n", PQgetvalue(res, i, 0), PQgetvalue(res, i, 1));
			}
						
			PQclear(res);
			res = PQexec_wrapper(delete_sql_smt.c_str());
			PQclear(res);
		} else {
			printf("Postgres DB cleaner, no history accounts will be removed\n");
			PQclear(res);
		}
					
	} catch (const char *error) {
		printf("Postgres DB cleaner, error executing history delete: %s\n", error);
	}
}



long current_time_sec()
{
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

long current_time_milli()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

