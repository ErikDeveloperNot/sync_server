#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <string>
#include <vector>
#include <condition_variable>
#include <cstring>

//network defaults
#define SERVER_BIND "ALL"
#define SERVER_PORT 9999
#define SERVICE_THREADS 10
#define SERVER_USE_SSL true
#define SERVER_CERT "cert.pem"
#define SERVER_KEY "key.pem"
#define SERVER_MAX_CONNECTIONS 200
#define SERVER_KEEP_ALIVE 30
#define SERVER_USE_KEEP_ALIVE_CLEANER false
#define SERVER_TCP_BACK_LOG 100
#define SERVER_START_THREAD_COUNT 25

#define MAX_ACCOUNT_PENDING_OPS 3

//database defaults 
#define DB_SERVER "localhost"
#define DB_PORT "5432"
#define DB_NAME "passvault"
#define DB_USER "user"
#define DB_PASS "password"
#define DB_MIN_CONNECTIONS 10
#define DB_MAX_CONNECTIONS 100
#define DB_CLEANER true
#define DB_CLEANER_INTERVAL 1
#define DB_CLEANER_PURGE_DAYS 30

//Redis defaults
#define REDIS_SERVER "redis"
#define REDIS_PORT 6379
#define REDIS_USER "redis"
#define REDIS_PASS "redis"
#define REDIS_MIN_CONNECTIONS 10
#define REDIS_MAX_CONNECTIONS 200
#define REDIS_CLEANER true
#define REDIS_CLEANER_INTERVAL 1
#define REDIS_CLEANER_PURGE_DAYS 30


//operation types
#define REGISTER_CONFIG "/PassvaultServiceRegistration/service/registerV1/sync-server"
#define DELETE_USER "/PassvaultServiceRegistration/service/deleteAccount/sync-server"
#define SYNC_INITIAL "/PassvaultServiceRegistration/service/sync-accounts/sync-initial"
#define SYNC_FINAL "/PassvaultServiceRegistration/service/sync-accounts/sync-final"


enum request_type { POST, GET };
enum operation_type { register_config, delete_user, sync_initial, sync_final };

enum store_type { postgres_store, redis_store };

		
struct User {
	char *account_uuid;
	char *account_password;
	long long account_last_sync;
	
	User() = default;
	User(const char *uuid, char *password, long long last_sync) : account_uuid{(char*)uuid}, account_password{password}, account_last_sync{last_sync}
	{
		account_uuid = (char*)malloc(strlen(uuid) + 1);
		account_password = (char*)malloc(strlen(password) + 1);
		strcpy(account_uuid, uuid);
		strcpy(account_password, password);
	}
};

struct User_info 
{
	User user;
	long lock_time;
	std::vector<std::condition_variable *> cv_vector;
	
	User_info() {}
	User_info(User user, long lock_time) :
	user{user},
	lock_time{lock_time}
	{}
};

//struct compare {
//	bool operator() (const User_info &lhs, const User_info &rhs) const {
//		return lhs.user.account_uuid > rhs.user.account_uuid;
//	}
//};

struct cmp_key
{
	bool operator()(char const *a, char const *b) const
	{
//		printf("cpmparing a: %s, b: %s\n", a, b);
		return strcmp(a, b) < 0;
	}
};


class Config
{
private:
	// Server settings
	std::string bind_address = SERVER_BIND;
	int bind_port = SERVER_PORT;
	int service_threads = SERVICE_THREADS;
	int max_service_threads = SERVICE_THREADS;
	bool ssl = SERVER_USE_SSL;
	std::string server_cert = SERVER_CERT;
	std::string server_key = SERVER_KEY;
	int server_max_connections = SERVER_MAX_CONNECTIONS;
	int server_keep_alive = SERVER_KEEP_ALIVE;
	bool server_use_keep_alive_cleaner = SERVER_USE_KEEP_ALIVE_CLEANER;
	int server_tcp_back_log = SERVER_TCP_BACK_LOG;
	
	int max_account_pending_ops = MAX_ACCOUNT_PENDING_OPS;
	
	// Postgres settings
	std::string db_server = DB_SERVER;
	std::string db_port = DB_PORT;
	std::string db_password = DB_PASS;
	std::string db_username = DB_USER;
	std::string db_name = DB_NAME;
	int db_min_connections = DB_MIN_CONNECTIONS;
	int db_max_connections = DB_MAX_CONNECTIONS;
	bool db_cleaner = DB_CLEANER;
	int db_cleaner_interval = DB_CLEANER_INTERVAL;
	int db_cleaner_purge_days = DB_CLEANER_PURGE_DAYS;
	int db_cleaner_history_purge_days = DB_CLEANER_PURGE_DAYS;
	
	// Redis settings
	std::string redis_server = REDIS_SERVER;
	int redis_port = REDIS_PORT;
	std::string redis_username = REDIS_USER;
	std::string redis_password = REDIS_PASS;
	int redis_min_connections = REDIS_MIN_CONNECTIONS;
	int redis_max_connections = REDIS_MAX_CONNECTIONS;
	bool redis_cleaner = REDIS_CLEANER;
	int redis_cleaner_interval = REDIS_CLEANER_INTERVAL;
	int redis_cleaner_purge_days = REDIS_CLEANER_PURGE_DAYS;
	int redis_cleaner_history_purge_days = REDIS_CLEANER_PURGE_DAYS;
	
	
	store_type storeType;
	void setKeyValue(char*, char*);
	
public:
	Config(char *config);
	~Config();
	
	void debugValues();
	
	void setBindAddress(const std::string& bind_address) {this->bind_address = bind_address;}
	void setBindPort(int bind_port) {this->bind_port = bind_port;}
	void setDbCleaner(bool db_cleaner) {this->db_cleaner = db_cleaner;}
	void setDbCleanerInterval(int db_cleaner_interval) {this->db_cleaner_interval = db_cleaner_interval;}
	void setDbCleanerPurgeDays(int db_cleaner_purge_days) {this->db_cleaner_purge_days = db_cleaner_purge_days;}
	void setDbCleanerHistoryPurgeDays(int db_cleaner_history_purge_days) {this->db_cleaner_history_purge_days = db_cleaner_history_purge_days;}
	void setDbName(const std::string& db_name) {this->db_name = db_name; storeType = postgres_store;}
	void setDbPassword(const std::string& db_password) {this->db_password = db_password;}
	void setDbPort(const std::string& db_port) {this->db_port = db_port;}
	void setDbServer(const std::string& db_server) {this->db_server = db_server;}
	void setDbUsername(const std::string& db_username) {this->db_username = db_username;}
	void setDbMaxConnections(int db_max_connections) {this->db_max_connections = db_max_connections;}
	void setDbMinConnections(int db_min_connections) {this->db_min_connections = db_min_connections;}
	void setServerCert(const std::string& server_cert) {this->server_cert = server_cert;}
	void setServerKey(const std::string& server_key) {this->server_key = server_key;}
	void setServiceThreads(int service_threads) {this->service_threads = service_threads;}
	void setMaxServiceThreads(int service_threads) {this->max_service_threads = service_threads;}
	void setSsl(bool ssl) {this->ssl = ssl;}
	void setServerMaxConnections(int server_max_connections) {this->server_max_connections = server_max_connections;}
	void setServerKeepAlive(int server_keep_alive) {this->server_keep_alive = server_keep_alive;}
	void setServerUseKeepAliveCleaner(bool server_use_keep_alive_cleaner) {this->server_use_keep_alive_cleaner = server_use_keep_alive_cleaner;}
	void setServerTcpBackLog(int server_tcp_back_log) {this->server_tcp_back_log = server_tcp_back_log;}
	void setMaxAccountPendingOps(int max_account_pending_ops) {this->max_account_pending_ops = max_account_pending_ops;}
	void setRedisCleaner(bool redis_cleaner) {this->redis_cleaner = redis_cleaner;}
	void setRedisCleanerInterval(int redis_cleaner_interval) {this->redis_cleaner_interval = redis_cleaner_interval;}
	void setRedisCleanerPurgeDays(int redis_cleaner_purge_days) {this->redis_cleaner_purge_days = redis_cleaner_purge_days;}
	void setRedisCleanerHistoryPurgeDays(int redis_cleaner_history_purge_days) {this->redis_cleaner_history_purge_days = redis_cleaner_history_purge_days;}
	void setRedisMaxConnections(int redis_max_connections) {this->redis_max_connections = redis_max_connections;}
	void setRedisMinConnections(int redis_min_connections) {this->redis_min_connections = redis_min_connections;}
	void setRedisPassword(const std::string& redis_password) {this->redis_password = redis_password;}
	void setRedisPort(int redis_port) {this->redis_port = redis_port;}
	void setRedisServer(const std::string& redis_server) {this->redis_server = redis_server; storeType = redis_store;}
	void setRedisUsername(const std::string& redis_username) {this->redis_username = redis_username;}
	
	
	bool isSsl() const {return ssl;}
//	const std::string& getBindAddress() const {return bind_address;}
	const char* getBindAddress() const {return bind_address.c_str();}
	const int getBindPort() const {return bind_port;}
	bool isDbCleaner() const {return db_cleaner;}
	int getDbCleanerInterval() const {return db_cleaner_interval;}
	int getDbCleanerPurgeDays() const {return db_cleaner_purge_days;}
	int getDbCleanerHistoryPurgeDays() const {return db_cleaner_history_purge_days;}
	const std::string& getDbName() const {return db_name;}
	const std::string& getDbPassword() const {return db_password;}
	const std::string& getDbPort() const {return db_port;}
	const std::string& getDbServer() const {return db_server;}
	const std::string& getDbUsername() const {return db_username;}
	int getDbMaxConnections() const {return db_max_connections;}
	int getDbMinConnections() const {return db_min_connections;}
	const std::string& getServerCert() const {return server_cert;}
	const std::string& getServerKey() const {return server_key;}
	int getServiceThreads() const {return service_threads;}
	int getMaxServiceThreads() const {return max_service_threads;}
	int getServerMaxConnections() const {return server_max_connections;}
	int getServerKeepAlive() const {return server_keep_alive;}
	bool isServerUseKeepAliveCleaner() const {return server_use_keep_alive_cleaner;}
	int getMaxAccountPendingOps() const {return max_account_pending_ops;}
	int getServerTcpBackLog() const {return server_tcp_back_log;}
	bool isRedisCleaner() const {return redis_cleaner;}
	int getRedisCleanerInterval() const {return redis_cleaner_interval;}
	int getRedisCleanerPurgeDays() const {return redis_cleaner_purge_days;}
	int getRedisCleanerHistoryPurgeDays() const {return redis_cleaner_history_purge_days;}
	int getRedisMaxConnections() const {return redis_max_connections;}
	int getRedisMinConnections() const {return redis_min_connections;}
	const std::string& getRedisPassword() const {return redis_password;}
	int getRedisPort() const {return redis_port;}
	const std::string& getRedisServer() const {return redis_server;}
	const std::string& getRedisUsername() const {return redis_username;}
	
	store_type getStoreType() const {return storeType;}
};



#endif // _CONFIG_H_
