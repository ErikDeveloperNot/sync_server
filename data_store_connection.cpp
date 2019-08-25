#include "data_store_connection.h"

#include <string>
#include <thread>



float account_format = 1.00;

data_store_connection::data_store_connection() : connected{false}
{
}

data_store_connection::~data_store_connection()
{
}


bool data_store_connection::initialize(const std::string &thread_name, Config *config) 
{
	t_id = thread_name;
	this->config = config;

	if (connect()) {
		createPreparedStatements();
	} else {
		printf("%s: was unable to create store connection, will retry at next request\n", t_id.c_str());
		return false;
	}
	
	return true;
}


void data_store_connection::createPreparedStatements()
{
	//not sure under waht conditions a prepare would fail so ignoring false return for now
	prepare(create_user_pre, CREATE_USER_PREPARE, 4);
	prepare(delete_user_pre, DELETE_USER_PREPARE, 1);
	prepare(update_last_sync_time_pre, UPDATE_USER_LAST_SYNC_PREPARE, 2);
	prepare(get_accounts_for_user_pre, GET_ACCOUNTS_FOR_USER_PREPARE, 1);
//	prepare(create_account_pre, CREATE_ACCOUNT_PREPARE, 8);
//	prepare(update_account_pre, UPDATE_ACCOUNT_PREPARE, 8);
	prepare(upsert_account_pre, UPSERT_ACCOUNT_PREPARE, 8);
	
	//should be 5 statements for this connection
	res = PQexec(conn, "select * from pg_prepared_statements");
	
	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		printf("Check prepare count failed: %s\n", PQerrorMessage(conn));
	} else {
		printf("Prepared %d statements\n", PQntuples(res));
	}
	
	PQclear(res);
}


bool data_store_connection::prepare(const char *prep_name, const std::string &prep_def, int count)
{
	bool success{true};
	res = PQprepare(conn, prep_name, prep_def.c_str(), count, NULL);
						
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("PQprepare %s, failed: %s\n", prep_name, PQerrorMessage(conn));
		success = false;
	}
	
	PQclear(res);
	return success;
}


bool data_store_connection::connect()
{
	int attempts{1};
	std::string conn_info{""};
	conn_info.append("hostaddr=").append(config->getDbServer());
	conn_info.append(" port=").append(config->getDbPort());
	conn_info.append(" dbname=").append(config->getDbName());
	conn_info.append(" user=").append(config->getDbUsername());
	conn_info.append(" password=").append(config->getDbPassword());
	int interval{1};
	
	do {
		conn = PQconnectdb(conn_info.c_str());

		if (PQstatus(conn) != CONNECTION_OK) {
			fprintf(stderr, "%s: Attempt %d, Connection to database failed: %s", t_id.c_str(), attempts, PQerrorMessage(conn));
			PQfinish(conn);
			//std::this_thread::sleep_for(std::chrono::milliseconds(CONNECT_ATTEMPTS_INTERVAL * 1000));
			std::this_thread::sleep_for(std::chrono::milliseconds(interval++ * 1000));
		} else {
			connected = true;
		}
		
	} while (!connected && attempts++ < CONNECT_ATTEMPTS);
	
	if (connected)
		return true;
	else
		return false;
}


std::vector<User> data_store_connection::getAllUsers()
{
	std::vector<User> users;

	res = PQexec(conn, GET_USERS_SQL.c_str());

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("%s: getAllUsers failed: %s\n", t_id.c_str(), PQerrorMessage(conn));
    } else {
		for (int i = 0; i < PQntuples(res); i++) {
			//PQgetvalue(res, i, j)
			users.emplace(users.begin(), std::string{PQgetvalue(res, i, 0)},
			std::string{PQgetvalue(res, i, 1)}, atoll(PQgetvalue(res, i, 2)));
		}
	}
	
	PQclear(res);
	return users;
}


std::map<std::string, Account> data_store_connection::get_accounts_for_user(std::string& user)
{
	std::map<std::string, Account> accounts;
	const char *values[1];
	values[0] = user.c_str();
	
	res = PQexecPrepared(conn, get_accounts_for_user_pre, 1, values, NULL, NULL, 0);
	
	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		printf("PQExec %s, failed: %s\n", get_accounts_for_user_pre, PQresStatus(PQresultStatus(res)));
		return accounts;
	}
	
	for (int i = 0; i < PQntuples(res); i++) {
		accounts.emplace(std::string{PQgetvalue(res, i, 0)}, Account{std::string{PQgetvalue(res, i, 0)}, 
			std::string{PQgetvalue(res, i, 1)},
			std::string{PQgetvalue(res, i, 2)}, std::string{PQgetvalue(res, i, 3)},
			std::string{PQgetvalue(res, i, 4)}, std::string{PQgetvalue(res, i, 5)},
			atoll(PQgetvalue(res, i, 6)), 
			((std::string{PQgetvalue(res, i, 7)}=="t") ? true : false)}); //TODO TODO revist bool postgres c lib
	}
	
	PQclear(res);
	return accounts;
}


bool data_store_connection::upsert_accounts_for_user(std::string& user, std::vector<Account>& accounts)
{
	int successCount{0};
	const char *values[8];
	
	for (Account &a : accounts) {
		values[2] = a.user_name.c_str();
		values[3] = a.password.c_str();
		values[4] = a.old_password.c_str();
		values[5] = a.url.c_str();
		values[6] = std::to_string(a.update_time).c_str();
		values[7] = ((a.deleted) ? "true\0" : "false\0");
		values[1] = user.c_str();
		values[0] = a.account_name.c_str();
		
//		res = PQexecPrepared(conn, update_account_pre, 8, values, NU	LL, NULL, 0);
		res = PQexecPrepared(conn, upsert_account_pre, 8, values, NULL, NULL, 0);
	
		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			printf("PQExec %s, failed: %s\n", upsert_account_pre, PQerrorMessage(conn));
		} else {
			successCount++;
		}
	
		PQclear(res);
  	}
	
	if (successCount == accounts.size())
		return true;
	else
		return false;
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
	
	res = PQexecPrepared(conn, create_user_pre, 4, values, NULL, NULL, 0);
	
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("PQExec %s, failed: %s\n", create_user_pre, PQerrorMessage(conn));
		success = false;
	}
	
	PQclear(res);
	return success;
}


bool data_store_connection::update_last_sync_for_user(std::string& user, long long lockTime)
{
	bool success{true};
	const char *values[2];
	values[0] = std::to_string(lockTime).c_str();
	values[1] = user.c_str();
	
	res = PQexecPrepared(conn, update_last_sync_time_pre, 2, values, NULL, NULL, 0);
	
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("PQExec %s, failed: %s\n", update_last_sync_time_pre, PQerrorMessage(conn));
		success = false;
	}
	
	PQclear(res);
	return success;
}


bool data_store_connection::delete_user(User& user)
{
	bool success{true};
	const char *values[1];
	values[0] = user.account_uuid.c_str();
	
	res = PQexecPrepared(conn, delete_user_pre, 1, values, NULL, NULL, 0);
	
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("PQExec %s, failed: %s\n", delete_user_pre, PQerrorMessage(conn));
		success = false;
	}
	
	PQclear(res);
	return success;
}

