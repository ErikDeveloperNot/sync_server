#ifndef _SYNC_HANDLER_H_
#define _SYNC_HANDLER_H_

#include "data_store_connection.h"
#include "Config.h"
#include "config_http.h"

#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <map>


static std::once_flag onceFlag;


const long LOCK_TIMEOUT = 30000L;

class sync_handler
{
private:
	std::string t_id;
	data_store_connection store;
	Config *config;
	std::map<std::string, User_info> &user_infos;
	std::mutex user_infos_lock;
	std::condition_variable user_infos_cv;
	config_http configHttp;
	
	long long current_time_ms();
	
	//operations
	std::string handle_register(std::string &request);
	std::string handle_config();
	std::string handle_delete(std::string &request);
	std::string handle_sync_initial(std::string &request);
	std::string handle_sync_final(std::string &request);
	
	
	long long lock_user(std::string & forUser);
	long long relock_user(std::string & forUser, long long lock);
	void unlock_user(std::string & forUser);
	
	bool verify_password(std::string & user, std::string & pw);
	bool hash_password(std::string &);
	void debug_user_infos();
public:
	sync_handler(const std::string &, Config *, std::map<std::string, User_info> &);
	~sync_handler();

	std::string handle_request(std::string &resource, std::string &request, request_type http_type);
	
};

#endif // _SYNC_HANDLER_H_
