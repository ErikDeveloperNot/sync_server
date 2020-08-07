#ifndef _DATA_STORE_CONNNECTION_H_
#define _DATA_STORE_CONNNECTION_H_

#include <libpq-fe.h>
#include "IDataStore.h"

#include <string>

#define CONNECT_ATTEMPTS  10
#define CONNECT_ATTEMPTS_INTERVAL 10



// SQL query
static const char GET_USERS_SQL[] = "SELECT * FROM users";
static const char ACCOUNT_HISTORY_SQL[] = "SELECT account_uuid, account_name FROM history WHERE insert_time < ";
static const char DELETE_HISTORY_SQL[] = "DELETE FROM history WHERE insert_time < ";
static const char OLD_ACCOUNT_SQL[] = "SELECT account_uuid, account_name FROM accounts WHERE update_time < ";
static const char DELETE_OLD_ACCOUNTS_SQL[] = "DELETE FROM accounts WHERE update_time < ";

// Prepared statements
static const char CREATE_USER_PREPARE[] = "INSERT INTO users VALUES ($1::text, $2::text, $3::bigint, $4::numeric)";
static const char DELETE_USER_PREPARE[] = "DELETE from users WHERE account_uuid = $1::text";
static const char UPDATE_USER_LAST_SYNC_PREPARE[] = "UPDATE Users SET account_last_sync = $1::bigint WHERE account_uuid = $2::text";
static const char GET_ACCOUNTS_FOR_USER_PREPARE[] = "SELECT * FROM accounts where account_uuid = $1::text";
//static const std::string CREATE_ACCOUNT_PREPARE = "INSERT INTO Accounts VALUES ($1::text, $2::text, $3::text, $4::text, $5::text, "
//													"$6::text, $7::bigint, $8::boolean)";
//static const std::string UPSERT_ACCOUNT_PREPARE = "INSERT INTO Accounts as a VALUES ($1::text, $2::text, $3::text, $4::text, $5::text, "
//													"$6::text, $7::bigint, $8::boolean) on conflict (account_name, account_uuid) DO UPDATE "
//													"SET user_name = EXCLUDED.user_name, password = EXCLUDED.password, old_password = "
//													"EXCLUDED.old_password, url = EXCLUDED.url, update_time = EXCLUDED.update_time, "
//													"deleted = EXCLUDED.deleted WHERE EXCLUDED.update_time > a.update_time";

static const char UPSERT_ACCOUNT_PREPARE_1[] = {"INSERT INTO Accounts as a VALUES "};
static const char UPSERT_ACCOUNT_PREPARE_2[] = {"on conflict (account_name, account_uuid) DO UPDATE "
													"SET user_name = EXCLUDED.user_name, password = EXCLUDED.password, old_password = "
													"EXCLUDED.old_password, url = EXCLUDED.url, update_time = EXCLUDED.update_time, "
													"deleted = EXCLUDED.deleted WHERE EXCLUDED.update_time > a.update_time"}; 

//static const std::string UPDATE_ACCOUNT_PREPARE = "UPDATE Accounts SET user_name = $1::text, password = $2::text, old_password = $3::text, " 
//											"url = $4::text, update_time = $5::bigint, deleted = $6::boolean WHERE account_UUID = $7::text "   
//											"AND account_name = $8::text";

static const char t[] = {"true"};
static const char f[] = {"false"};

class data_store_connection : public IDataStore
{
private:
	Config *config;
	char sync_buf[32];
	int max_connections;
	
	const char *create_user_pre = "create_user_pre";
	const char *delete_user_pre = "delete_user_pre";
	const char *update_last_sync_time_pre = "update_last_sync_time_prepare";
	const char *get_accounts_for_user_pre = "get_accounts_for_user_pre";
//	const char *create_account_pre = "create_account_pre";
//	const char *update_account_pre = "update_account_pre";
//	const char *upsert_account_pre = "upsert_account_pre";
	
	PGconn * get_connection();
	void release_connection(PGconn *);
	
	bool createPreparedStatements(PGconn *);
	bool prepare(PGconn *, const char *prep_name, const char *prep_def, int count);
	PGconn* connect();
//	PGconn* reset_connection(PGconn *);
	PGresult* PQexecPrepared_wrapper(const char *pre, const char *args[], int argc);
	PGresult* PQexec_wrapper(const char *sql);
	
//	void start_connection_manager();
//	void start_data_cleaner();
	
	// override called by connection manager thread
	virtual void closeConn(void*);
	// override call by account cleaner thread
	virtual void deleteOldUsers(long time_ms);
	// override call by account cleaner thread
	virtual void deleteHistory(long time_sec);
	
	
	inline unsigned int increase_buffer(unsigned int needed, unsigned int sz, unsigned int indx, char *& txt) 
	{
		if (needed + 20 > sz - indx) {
//printf("\nREALLOC NEEDED\n");
			sz = (needed + (unsigned int)(sz * 1.25));
			sz += sz % sizeof(char*);
			txt = (char*) realloc(txt, sz);
		}
		
		return sz;
	}
	
	
public:
	data_store_connection();
	virtual ~data_store_connection();

	virtual bool initialize(Config *);
	
	virtual std::vector<User> getAllUsers();
	virtual bool createUser(User &user);
	virtual bool delete_user(User &user);
	virtual std::map<char *, Account, cmp_key> get_accounts_for_user(const char *user, Heap_List * heap);
	virtual bool upsert_accounts_for_user(const char *user, std::vector<Account> &accounts);
	virtual bool update_last_sync_for_user(const char *user, long long lockTime);
	
};

#endif // _DATA_STORE_CONNNECTION_H_
