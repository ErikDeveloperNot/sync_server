#ifndef _DATA_STORE_CONNNECTION_H_
#define _DATA_STORE_CONNNECTION_H_

#include <libpq-fe.h>
#include "Config.h"

#include <string>
#include <vector>
#include <map>
#include <mutex>

#define CONNECT_ATTEMPTS  10
#define CONNECT_ATTEMPTS_INTERVAL 10



struct Account {
	std::string account_name;
	std::string account_uuid;
	std::string user_name;
	std::string password;
	std::string old_password;
	std::string url;
	long long update_time;
	bool deleted;
	
	Account() = default;
	Account(std::string account_name, std::string uuid, std::string user_name, std::string pass,
			std::string old_pass, std::string url, long long update, bool deleted) :
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


// SQL query
static const std::string GET_USERS_SQL = "SELECT * FROM users";
static const std::string ACCOUNT_HISTORY_SQL = "SELECT account_uuid, account_name FROM history WHERE insert_time < ";
static const std::string DELETE_HISTORY_SQL = "DELETE FROM history WHERE insert_time < ";
static const std::string OLD_ACCOUNT_SQL = "SELECT account_uuid, account_name FROM accounts WHERE update_time < ";
static const std::string DELETE_OLD_ACCOUNTS_SQL = "DELETE FROM accounts WHERE update_time < ";

// Prepared statements
static const std::string CREATE_USER_PREPARE = "INSERT INTO users VALUES ($1::text, $2::text, $3::bigint, $4::numeric)";
static const std::string DELETE_USER_PREPARE = "DELETE from users WHERE account_uuid = $1::text";
static const std::string UPDATE_USER_LAST_SYNC_PREPARE = "UPDATE Users SET account_last_sync = $1::bigint WHERE account_uuid = $2::text";
static const std::string GET_ACCOUNTS_FOR_USER_PREPARE = "SELECT * FROM accounts where account_uuid = $1::text";
//static const std::string CREATE_ACCOUNT_PREPARE = "INSERT INTO Accounts VALUES ($1::text, $2::text, $3::text, $4::text, $5::text, "
//													"$6::text, $7::bigint, $8::boolean)";
//static const std::string UPSERT_ACCOUNT_PREPARE = "INSERT INTO Accounts as a VALUES ($1::text, $2::text, $3::text, $4::text, $5::text, "
//													"$6::text, $7::bigint, $8::boolean) on conflict (account_name, account_uuid) DO UPDATE "
//													"SET user_name = EXCLUDED.user_name, password = EXCLUDED.password, old_password = "
//													"EXCLUDED.old_password, url = EXCLUDED.url, update_time = EXCLUDED.update_time, "
//													"deleted = EXCLUDED.deleted WHERE EXCLUDED.update_time > a.update_time";

static const std::string UPSERT_ACCOUNT_PREPARE_1 = "INSERT INTO Accounts as a VALUES ";
static const std::string UPSERT_ACCOUNT_PREPARE_2 = "on conflict (account_name, account_uuid) DO UPDATE "
													"SET user_name = EXCLUDED.user_name, password = EXCLUDED.password, old_password = "
													"EXCLUDED.old_password, url = EXCLUDED.url, update_time = EXCLUDED.update_time, "
													"deleted = EXCLUDED.deleted WHERE EXCLUDED.update_time > a.update_time"; 

//static const std::string UPDATE_ACCOUNT_PREPARE = "UPDATE Accounts SET user_name = $1::text, password = $2::text, old_password = $3::text, " 
//											"url = $4::text, update_time = $5::bigint, deleted = $6::boolean WHERE account_UUID = $7::text "   
//											"AND account_name = $8::text";



class data_store_connection
{
private:
	Config *config;
//	std::string t_id;
	
//	PGconn     *conn;
//  PGresult   *res;
//	bool connected;
	int current_connections;
	int max_connections;
	std::vector<PGconn *> connections;
	std::mutex connections_mutex;
	std::condition_variable connections_cv;
	
	const char *create_user_pre = "create_user_pre";
	const char *delete_user_pre = "delete_user_pre";
	const char *update_last_sync_time_pre = "update_last_sync_time_prepare";
	const char *get_accounts_for_user_pre = "get_accounts_for_user_pre";
//	const char *create_account_pre = "create_account_pre";
//	const char *update_account_pre = "update_account_pre";
	const char *upsert_account_pre = "upsert_account_pre";
	
	PGconn * get_connection();
	void release_connection(PGconn *);
	
	bool createPreparedStatements(PGconn *);
	bool prepare(PGconn *, const char *prep_name, const std::string &prep_def, int count);
	PGconn* connect();
//	PGconn* reset_connection(PGconn *);
	PGresult* PQexecPrepared_wrapper(const char *pre, const char *args[], int argc);
	PGresult* PQexec_wrapper(const char *sql);
	
	void start_connection_manager();
	void start_data_cleaner();
	
public:
	data_store_connection();
	~data_store_connection();

	bool initialize(Config *);
	
	std::vector<User> getAllUsers();
	bool createUser(User &user);
	bool delete_user(User &user);
	std::map<std::string, Account> get_accounts_for_user(std::string &user);
	bool upsert_accounts_for_user(std::string &user, std::vector<Account> &accounts);
	bool update_last_sync_for_user(std::string &user, long long lockTime);
};

#endif // _DATA_STORE_CONNNECTION_H_
