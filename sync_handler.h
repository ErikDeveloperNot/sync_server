#ifndef _SYNC_HANDLER_H_
#define _SYNC_HANDLER_H_

#include "data_store_connection.h"
#include "Config.h"
#include "config_http.h"
#include "jsonP_parser.h"

#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <map>


static std::once_flag onceFlag;


const long LOCK_TIMEOUT = 30000L;

static char bad_request[] = "{ error: \"Bad Request\" }";
static char account_deleted[] = "{ msg: \"Account has been deleted\" }";
static char sync_final_resp[] = "{ msg : \"Sync Final Complete\" }";

//json path constants
static long default_response_code = 0;
static char delim[] = {"/"};
static char proto[] = {"https"};
static char email_path[] = {"/email"};
static char user_path[] = {"/user"};
static char pass_path[] = {"/password"};
static char sync_path[] = {"PassvaultServiceRegistration/service/sync-accounts"};
static char bucket_key[] = {"bucket"};
static char port_key[] = {"port"};
static char protocol_key[] = {"protocol"};
static char user_key[] = {"userName"};
static char pass_key[] = {"password"};
static char oldPass_key[] = {"oldPassword"};
static char server_key[] = {"server"};
static char lockTime_key[] = {"lockTime"};
static char responseCode_key[] = {"responseCode"};
static char accounts_key[] = {"accounts"};
static char accounts_path[] = {"/accounts"};
static char lockTime_path[] = {"/lockTime"};
static char sendAccountsToServerList_key[] = {"sendAccountsToServerList"};
static char accountsToSendBackToClient_key[] = {"accountsToSendBackToClient"};
static char accountName_key[] = {"accountName"};
static char deleted_key[] = {"deleted"};
static char updateTime_key[] = {"updateTime"};
static char url_key[] = {"url"};


class sync_handler
{
private:
	std::string t_id;
	data_store_connection &store;
	Config *config;
	std::map<char *, User_info, cmp_key> &user_infos;
	
	// lock/cv used for user_infos map
	std::mutex user_infos_lock;
	std::condition_variable user_infos_cv;
	
	// lock/cv used for this thread for user_infos cv vector
	std::mutex handler_mutex;
	std::condition_variable handler_cv;
	
	config_http configHttp;
	error jsonP_err;
	
	long long current_time_ms();
	long current_time_sec();
	
	//operations
	char * handle_register(char *request);
	char * handle_config();
	char * handle_delete(char *request);
	char * handle_sync_initial(char *request);
	char * handle_sync_final(char *request);
	
	// lock methods deprecated
	long lock_user(const char *forUser);
	long relock_user(const char *forUser, long lock);
	void unlock_user(const char *forUser, long lockTime);
	
	bool verify_email(const char *);
	bool verify_password(const char *user, const char *pw);
	bool hash_password(const char *, char *);
	void debug_user_infos();
public:
	sync_handler(const std::string &, Config *, std::map<char *, User_info, cmp_key> &, data_store_connection &store);
	~sync_handler();

	char * handle_request(operation_type op_type, char *request, request_type http_type);
};

#endif // _SYNC_HANDLER_H_
