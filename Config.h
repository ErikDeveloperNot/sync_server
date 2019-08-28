#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <string>

//network defaults
#define SERVER_BIND "ALL"
#define SERVER_PORT 9999
#define SERVICE_THREADS 10
#define SERVER_USE_SSL true
#define SERVER_CERT "cert.pem"
#define SERVER_KEY "key.pem"
#define SERVER_MAX_CONNECTIONS 200
#define SERVER_KEEP_ALIVE 30
#define SERVER_USE_KEEP_ALIVE_CLEANER true

//database defaults
#define DB_SERVER "localhost"
#define DB_PORT "5432"
#define DB_NAME "passvault"
#define DB_USER "user"
#define DB_PASS "password"
#define DB_CLEANER true
#define DB_CLEANER_INTERVAL 1
#define DB_CLEANER_PURGE_DAYS 30

//operation types
#define REGISTER_CONFIG "/PassvaultServiceRegistration/service/registerV1/sync-server"
#define DELETE_USER "/PassvaultServiceRegistration/service/deleteAccount/sync-server"
#define SYNC_INITIAL "/PassvaultServiceRegistration/service/sync-accounts/sync-initial"
#define SYNC_FINAL "/PassvaultServiceRegistration/service/sync-accounts/sync-final"


enum request_type { POST, GET };

		
struct User {
	std::string account_uuid;
	std::string account_password;
	long long account_last_sync;
	
	User() {}
	User(std::string uuid, std::string password, long long last_sync) : 
	account_uuid{uuid},
	account_password{password},
	account_last_sync{last_sync} 
	{}
};

struct User_info 
{
	User user;
	long long lock_time;
	
	User_info() {}
	User_info(User user, long long lock_time) :
	user{user},
	lock_time{lock_time}
	{}
};

//struct compare {
//	bool operator() (const User_info &lhs, const User_info &rhs) const {
//		return lhs.user.account_uuid > rhs.user.account_uuid;
//	}
//};


class Config
{
private:
	std::string bind_address = SERVER_BIND;
	int bind_port = SERVER_PORT;
	int service_threads = SERVICE_THREADS;
	bool ssl = SERVER_USE_SSL;
	std::string server_cert = SERVER_CERT;
	std::string server_key = SERVER_KEY;
	int server_max_connections = SERVER_MAX_CONNECTIONS;
	int server_keep_alive = SERVER_KEEP_ALIVE;
	bool server_use_keep_alive_cleaner = SERVER_USE_KEEP_ALIVE_CLEANER;
	
	std::string db_server = DB_SERVER;
	std::string db_port = DB_PORT;
	std::string db_password = DB_PASS;
	std::string db_username = DB_USER;
	std::string db_name = DB_NAME;
	bool db_cleaner = DB_CLEANER;
	int db_cleaner_interval = DB_CLEANER_INTERVAL;
	int db_cleaner_purge_days = DB_CLEANER_PURGE_DAYS;
	
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
	void setDbName(const std::string& db_name) {this->db_name = db_name;}
	void setDbPassword(const std::string& db_password) {this->db_password = db_password;}
	void setDbPort(const std::string& db_port) {this->db_port = db_port;}
	void setDbServer(const std::string& db_server) {this->db_server = db_server;}
	void setDbUsername(const std::string& db_username) {this->db_username = db_username;}
	void setServerCert(const std::string& server_cert) {this->server_cert = server_cert;}
	void setServerKey(const std::string& server_key) {this->server_key = server_key;}
	void setServiceThreads(int service_threads) {this->service_threads = service_threads;}
	void setSsl(bool ssl) {this->ssl = ssl;}
	void setServerMaxConnections(int server_max_connections) {this->server_max_connections = server_max_connections;}
	void setServerKeepAlive(int server_keep_alive) {this->server_keep_alive = server_keep_alive;}
	void setServerUseKeepAliveCleaner(bool server_use_keep_alive_cleaner) {this->server_use_keep_alive_cleaner = server_use_keep_alive_cleaner;}
	
	bool isSsl() const {return ssl;}
	const std::string& getBindAddress() const {return bind_address;}
	int getBindPort() const {return bind_port;}
	bool isDbCleaner() const {return db_cleaner;}
	int getDbCleanerInterval() const {return db_cleaner_interval;}
	int getDbCleanerPurgeDays() const {return db_cleaner_purge_days;}
	const std::string& getDbName() const {return db_name;}
	const std::string& getDbPassword() const {return db_password;}
	const std::string& getDbPort() const {return db_port;}
	const std::string& getDbServer() const {return db_server;}
	const std::string& getDbUsername() const {return db_username;}
	const std::string& getServerCert() const {return server_cert;}
	const std::string& getServerKey() const {return server_key;}
	int getServiceThreads() const {return service_threads;}
	int getServerMaxConnections() const {return server_max_connections;}
	int getServerKeepAlive() const {return server_keep_alive;}
	bool isServerUseKeepAliveCleaner() const {return server_use_keep_alive_cleaner;}
	
};



#endif // _CONFIG_H_
