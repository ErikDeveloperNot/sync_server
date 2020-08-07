#ifndef _REDISSTORE_H
#define _REDISSTORE_H

#include "IDataStore.h" // Base class: IDataStore

#include "hiredis.h"



#define REDIS_MAX_ATTEMPS 3

// Account fields
#define ACCOUNT_NAME		account_name
#define USER_NAME			user_name
#define PASSWORD			password
#define OLD_PASSWORD		old_password
#define URL					url_key
#define UPDATE_TIME		update_time
#define DELETED			deleted

#define ACCOUNT_MEMBERS	14

static const char* REDIS_CMD_TEST = "PING";
static const char* REDIS_CMD_AUTH = "AUTH %s %s";

static const char* REDIS_CMD_DELACCOUNTLIST = "DEL %s.accounts";
static const char* REDIS_CMD_DELACCOUNT = "DEL %s.account.%s";
static const char* REDIS_CMD_GETACCOUNT = "HGETALL %s.account.%s";
static const char* REDIS_CMD_SETACCOUNT = 
	"HMSET %s.account.%s account_name %s user_name %s password %s old_password %s url %s update_time %lld deleted %lld";
//static const char* REDIS_CMD_GETACCOUNTS = "LRANGE %s.accounts 0 -1";
static const char* REDIS_CMD_GETACCOUNTS = "HKEYS %s.accounts";
static const char* REDIS_CMD_ADDTOACCOUNTS = "HSET %s.accounts %s 1";
static const char* REDIS_CMD_DELETEFROMACCOUNTS = "HDEL %s.accounts %s";

static const char* REDIS_CMD_GETUSERS = "HGETALL USERS";
static const char* REDIS_CMD_GETUSERKEYS = "HKEYS USERS";
static const char* REDIS_CMD_USEREXISTS = "HEXISTS USERS %s";
static const char* REDIS_CMD_CREATEUSER = "HSET USERS %s %s";
static const char* REDIS_CMD_DELUSER = "HDEL USERS %s";

static const char* REDIS_CMD_DELUSERSYNC = "DEL %s.lastSync";
static const char* REDIS_CMD_SETLASTSYNC = "SET %s.lastSync %lld";
static const char* REDIS_CMD_GETUSERSYNC = "GET %s.lastSync";

//static const char* REDIS_CMD_DANGLINGACCOUNT = "keys *@*.*.account.*";



class RedisStore : public IDataStore
{

private:
	int maxConnections;
	
	redisContext *createConnection();
	redisContext *getConnection(bool);
	redisContext *testConnection(redisContext *);
	
	bool eatPipelineReplies(size_t cnt, redisContext*);
	
	void releaseConnection(redisContext*);
	bool areReplyErrors(redisReply*, redisContext*);
	std::vector<User> getAllUsers(bool);
	
	// override called by connection manager thread
	virtual void closeConn(void*);
	// override call by account cleaner thread
	virtual void deleteOldUsers(long time_ms);
	// override call by account cleaner thread
	virtual void deleteHistory(long time_sec);
	
	
	inline long long current_time_ms()
	{
		return std::chrono::system_clock::now().time_since_epoch().count()/1000;
	}
	
	
public:
	RedisStore();
	virtual ~RedisStore();

	virtual bool createUser(User& user);
	virtual bool delete_user(User& user);
	virtual std::vector<User> getAllUsers();
	virtual std::map<char*,Account,cmp_key> get_accounts_for_user(const char* user, Heap_List* heap);
	virtual bool initialize(Config* );
	virtual bool update_last_sync_for_user(const char* user, long long lastSync);
	virtual bool upsert_accounts_for_user(const char* user, std::vector<Account>& accounts);
};

#endif // REDISSTORE_H
