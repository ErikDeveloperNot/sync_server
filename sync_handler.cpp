#include "sync_handler.h"
#include "json_parser.h"
#include "register_server_exception.h"

#include "openssl/evp.h"

#include <thread>
#include <vector>
#include <map>
#include <cstring>


sync_handler::sync_handler(const std::string &thread_id, Config *config, 
							std::map<std::string, User_info> &user_infos, data_store_connection &store) :
t_id{thread_id},
config{config},
user_infos{user_infos},
store{store}
{
	std::call_once(onceFlag, [&]() {
		printf("%s: initializing the user_infos set\n", t_id.c_str());
		std::vector<User> users = store.getAllUsers();
		
		for (User x : users) {
			user_infos[x.account_uuid] = User_info{x, 0L};
		}
		
		debug_user_infos();
	});
	
}

sync_handler::~sync_handler()
{
}


std::string sync_handler::handle_request(std::string &resource, std::string &request, request_type http_type)
{
	try {
		if (resource == REGISTER_CONFIG) {
			if (http_type == request_type::POST) {
				return handle_register(request);
			} else {
				return handle_config();
			}
		} else if (resource == DELETE_USER) {
			return handle_delete(request);
		} else if (resource == SYNC_INITIAL) {
			return handle_sync_initial(request);
		} else if (resource == SYNC_FINAL) {
			return handle_sync_final(request);
		} else {
			return "{ error: \"Bad Request\" }";;
		}
	} catch (const char *error) {
		throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
	}
}


std::string sync_handler::handle_register(std::string& request)
{
//	printf("request:\n%s\n", request.c_str());
	json_parser parser{};
	register_config_request registerConfigReq;
	
	try {
		registerConfigReq = parser.parse_register_config(request);
	} catch (json_parser_exception &ex) {
//		throw register_server_exception{STATUS_400};
		throw register_server_exception{configHttp.build_reply(HTTP_400, close_con)};
	}
	
	//hash password
	std::string hashedPassword = registerConfigReq.password;
	
	if (!hash_password(hashedPassword)) {
		printf("Failed to hash password for user: %s\npass: %s\n", registerConfigReq.email.c_str(),
				registerConfigReq.password.c_str());
	}
	
	long long current_t = current_time_ms();
	//create the account
	User user{registerConfigReq.email, hashedPassword, 1L};
	User_info userInfo{user, current_t};
		
	//lock and check for account
	user_infos_lock.lock();
	
//debug_user_infos();	
	
	if (user_infos.count(registerConfigReq.email) < 1) {
		user_infos[user.account_uuid] = userInfo;
		user_infos_lock.unlock();
		user_infos_cv.notify_one();

		if (!store.createUser(user)) {
			//account creation failed
			user_infos_lock.lock();
			user_infos.erase(user.account_uuid);
			user_infos_lock.unlock();
			user_infos_cv.notify_one();
//			throw register_server_exception{STATUS_500_SERVER_ERROR};
			throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
		} /*else {
			// unlock user
			unlock_user(user.account_uuid, current_t);
		}*/
	} else {
		//user already exist return
		user_infos_lock.unlock();
		user_infos_cv.notify_one();
//		throw register_server_exception{STATUS_500_USER_EXISTS};
		throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
	}
	

	
	std::string prot{"https"};
	register_config_reply reply{config->getBindAddress(), config->getBindPort(), prot, 
								registerConfigReq.email, registerConfigReq.password};
	
	return reply.serialize();;
}


std::string sync_handler::handle_config()
{
	std::string prot{"https"};
	std::string email{"xxx@xxx.com"};
	std::string pass{"xxx"};
	register_config_reply reply{config->getBindAddress(), config->getBindPort(), prot, 
								email, pass};
	
	return reply.serialize();;
}


std::string sync_handler::handle_delete(std::string& request)
{
	json_parser parser{};
	register_config_request registerConfigReq;
	
	try {
		registerConfigReq = parser.parse_delete_account(request);
	} catch (json_parser_exception &ex) {
		throw register_server_exception{configHttp.build_reply(HTTP_400, close_con)};
	}

//	long long current_t = lock_user(registerConfigReq.email);
	
	if (!verify_password(registerConfigReq.email, registerConfigReq.password)) {
//		unlock_user(registerConfigReq.email, current_t);
		throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
	}
	
	if (!store.delete_user(user_infos[registerConfigReq.email].user)) {
		printf("Error deleting account %s from the database\n", registerConfigReq.email.c_str());
//		unlock_user(registerConfigReq.email, current_t);
		throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
	}
	
	user_infos_lock.lock();
	user_infos.erase(registerConfigReq.email);
	user_infos_lock.unlock();
	user_infos_cv.notify_one();
	
	return "{ msg: \"Account has been deleted\" }";
}


std::string sync_handler::handle_sync_initial(std::string& request)
{
//	printf("%s\n", request.c_str());
	json_parser parser{};
	sync_initial_request syncReq;
	
	try {
		syncReq = parser.parse_sync_initial(request);
	} catch (json_parser_exception &ex) {
		throw register_server_exception{configHttp.build_reply(HTTP_400, close_con)};
	}

	sync_initial_response syncResp;
//	syncResp.lockTime = lock_user(syncReq.registerConfigReq.email);
	
	//still need to add the lockTime to the clients since they parse for it
	syncResp.lockTime = current_time_sec();
	syncResp.responseCode = 0; //TODO TODO -  check on this
	
	if (!verify_password(syncReq.registerConfigReq.email, syncReq.registerConfigReq.password)) {
//		unlock_user(syncReq.registerConfigReq.email, syncResp.lockTime);
		throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
	}
	
	std::map<std::string, Account> accounts = store.get_accounts_for_user(syncReq.registerConfigReq.email);
	
//	for (auto &a : accounts)
//		printf("%s, %s, %lld\n", a.first.c_str(), a.second.account_uuid.c_str(), a.second.update_time);

	/*
	 * for each account sent from client check with store list to see if send back/send to/nothing
	 */
	for (sync_initial_account &a : syncReq.accounts) {
		if (accounts.count(a.account_name) > 0) {
			if (a.update_time > accounts[a.account_name].update_time) {
				//client version more recent - add to client send back to server
				syncResp.sendAccountsToServerList.push_back(a.account_name);
			} else if (accounts[a.account_name].update_time > a.update_time) {
				//server version more recent - add to send back to client
				syncResp.accountsToSendBackToClient.push_back(accounts[a.account_name]);
			} 
			
			accounts.erase(a.account_name);
		} else {
			//no version exists on server - add to client send back to server
			syncResp.sendAccountsToServerList.push_back(a.account_name);
		}
	}
	
	//whatever is left in map should be sent back to client
	for (auto &a : accounts)
		syncResp.accountsToSendBackToClient.push_back(a.second);
	
	return syncResp.serialize();
}


std::string sync_handler::handle_sync_final(std::string& request)
{
//	printf("SYNC FINAL:\n%s\n", request.c_str());
	json_parser parser;
	sync_final_request syncFinal;
	
	try {
		syncFinal = parser.parse_sync_final(request);
	} catch (json_parser_exception &ex) {
		throw register_server_exception{configHttp.build_reply(HTTP_400, close_con)};
	}
	
	if (!verify_password(syncFinal.user, syncFinal.password)) {
//		unlock_user(syncFinal.user, syncFinal.lockTime);
		throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
	}
	
//	long long relockTime = relock_user(syncFinal.user, syncFinal.lockTime);
	long long relockTime = current_time_sec();

	//update store
	if (store.upsert_accounts_for_user(syncFinal.user, syncFinal.accounts)) {
		if (!store.update_last_sync_for_user(syncFinal.user, relockTime)) {
			printf("Failed to update last SyncTime for user: %s in syncFinal\n", syncFinal.user.c_str());
//			unlock_user(syncFinal.user, relockTime);
			throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
		}
	} else {
		//means there was at least 1 failure, so dont update last sync time so another sync can be done
		printf("Failed to update accounts for user: %s in syncFinal\n", syncFinal.user.c_str());
//		unlock_user(syncFinal.user, relockTime);
		throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
	}
	
	//unlock user
//	unlock_user(syncFinal.user, relockTime);
	
	return "{ msg : \"Sync Final Complete\" }";
}


long long sync_handler::current_time_ms()
{
//	printf("-----------%lld\n", std::chrono::system_clock::now().time_since_epoch().count()/1000);
	return std::chrono::system_clock::now().time_since_epoch().count()/1000;
}


long sync_handler::current_time_sec()
{
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}


/*
 *  deprecated -  no longer needed, instead use upsert with on conflict
 */
long sync_handler::lock_user(std::string& forUser)
{
	std::unique_lock<std::mutex> lock(user_infos_lock);
	
	if (user_infos.count(forUser) < 1) {
		lock.unlock();
		user_infos_cv.notify_one();
		throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
	}
	
	if (!user_infos[forUser].cv_vector.empty()) {
		printf("%s - waiting for lock for user: %s\n", t_id.c_str(), forUser.c_str());
		auto sec = std::chrono::seconds(30);
		
		// add hndler/thread cond variable to user queue, release user_info_lock and wait on cv. TODO - add timeout later
		std::unique_lock<std::mutex> temp_lock(handler_mutex);
		user_infos[forUser].cv_vector.push_back(&handler_cv);
		lock.unlock();
		user_infos_cv.notify_one();
		
//		while (!handler_cv.wait(temp_lock, [&]() {
//			lock.lock();
//			
//			if (user_infos[forUser].cv_vector[0] != &handler_cv) {
//				lock.unlock();
//				user_infos_cv.notify_one();
//				return false;
//			} else {
//				return true;
//			}
//		})) {}

		handler_cv.wait(temp_lock);
		printf("%s - notified waiting for %s\n", t_id.c_str(), forUser.c_str());
		lock.lock();
		
		//should never see this
		if (user_infos[forUser].cv_vector[0] != &handler_cv) {
			printf("%s - %s notified but wrong cv is in front\n", t_id.c_str(), forUser.c_str());
			
			std::vector<std::condition_variable *>::iterator it = user_infos[forUser].cv_vector.begin();
			for (it; it != user_infos[forUser].cv_vector.end(); it++)
				if (*it == &handler_cv)
					break;
					
			if (it != user_infos[forUser].cv_vector.end())
				user_infos[forUser].cv_vector.erase(it);
			
			lock.unlock();
			user_infos_cv.notify_one();
			
			throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
		}

//		if (handler_cv.wait_for(temp_lock, sec) == std::cv_status::timeout) {
//			printf("%s - timed out waiting for lock for user: %s\n", t_id.c_str(), forUser.c_str());
//			throw register_server_exception{configHttp.build_reply(HTTP_503, close_con)};
//		}
	} else {
		user_infos[forUser].cv_vector.push_back(&handler_cv);
	}



	long current_lock = current_time_sec();

//printf("######### %lld : %lld : %lld\n", current_lock, user_infos[forUser].lock_time, LOCK_TIMEOUT);
//	if (current_lock - user_infos[forUser].lock_time < LOCK_TIMEOUT) {
//		printf("%s - waiting for lock for user: %s\n", t_id.c_str(), forUser.c_str());
//		auto sec = std::chrono::seconds(30);
//		auto milliSec = std::chrono::milliseconds(200);
//		
//		while (!user_infos_cv.wait_for(lock, milliSec, [&]() {
//			current_lock = current_time_ms();
//			printf("%s - check if \(%lld - %lld = %lld\) > %lld\n", t_id.c_str(), current_lock, user_infos[forUser].lock_time,
//					(current_lock - user_infos[forUser].lock_time), LOCK_TIMEOUT);
//			return (current_lock - user_infos[forUser].lock_time > LOCK_TIMEOUT);
//		})) {}

		// add hndler/thread cond variable to user queue, release user_info_lock and wait on cv. TODO - add timeout later
//		std::unique_lock<std::mutex> temp_lock(handler_mutex);
////		user_infos[forUser].cv_queue.push(&handler_cv);
//		lock.unlock();
//		user_infos_cv.notify_one();
//		
//		if (handler_cv.wait_for(temp_lock, sec) == std::cv_status::timeout) {
//			printf("%s - timed out waiting for lock for user: %s\n", t_id.c_str(), forUser.c_str());
//			throw register_server_exception{configHttp.build_reply(HTTP_503, close_con)};
//		}
//
//		printf("%s - got lock for user %s\n", t_id.c_str(), forUser.c_str());
//		lock.lock();
		
		//if still locked throw 503
//		current_lock = current_time_ms();
//		if (current_lock - user_infos[forUser].lock_time < LOCK_TIMEOUT) {
//			lock.unlock();
//			user_infos_cv.notify_one();
////			throw register_server_exception{STATUS_503.c_str()};
//			throw register_server_exception{configHttp.build_reply(HTTP_503, close_con)};
//		}
//	}
	
	user_infos[forUser].lock_time = current_lock;
	lock.unlock();
	user_infos_cv.notify_one();
	
	return current_lock;
}


/*
 *  deprecated -  no longer needed, instead use upsert with on conflict
 */
long sync_handler::relock_user(std::string& forUser, long userLock)
{
	long toReturn{0};
	std::unique_lock<std::mutex> lock(user_infos_lock);
	
	if (user_infos.count(forUser) < 1) {
		lock.unlock();
		user_infos_cv.notify_one();

//		throw register_server_exception{STATUS_401};
		throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
	}
	
	if (userLock != user_infos[forUser].lock_time) {
		//slow client or server, fail
		throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
	} else {
		toReturn = current_time_sec();
		user_infos[forUser].lock_time = toReturn;
	}
	
	lock.unlock();
	user_infos_cv.notify_one();
	
	return toReturn;
}

/*
 *  deprecated -  no longer needed, instead use upsert with on conflict
 */
void sync_handler::unlock_user(std::string& forUser, long lockTime)
{
	user_infos_lock.lock();
//	user_infos[forUser].lock_time = 0;
	
	if (lockTime != user_infos[forUser].lock_time) {
		printf("%s - Tried to unlock for user: %s, but locktimes dont match\n", t_id.c_str(), forUser.c_str());
	} else {
		user_infos[forUser].lock_time = 0;
	
	
//	std::vector<std::condition_variable *>::iterator it = user_infos[forUser].cv_vector.begin();
	
//	for (it; it != user_infos[forUser].cv_vector.end(); it++) {
//std::cout << "*it: " << *it << ", &handler_cv: " << &handler_cv << std::endl;
//std::cout << "&it: " << &it << ", &handler_cv: " << &handler_cv << std::endl;
//		if (*it == &handler_cv)
//			break;
//		
//	}
//					
//	if (it != user_infos[forUser].cv_vector.end())
//		user_infos[forUser].cv_vector.erase(it);
		
		if (!user_infos[forUser].cv_vector.empty()) {
			user_infos[forUser].cv_vector.erase(user_infos[forUser].cv_vector.begin());
			
			if (!user_infos[forUser].cv_vector.empty()) 
				user_infos[forUser].cv_vector[0]->notify_one();
		}
	}
	
	user_infos_lock.unlock();
	user_infos_cv.notify_one();
}


void sync_handler::debug_user_infos()
{
	for (auto &k : user_infos) {
		printf("%s, %lld\n", k.first.c_str(), user_infos[k.first].lock_time);
	}
}


bool sync_handler::verify_password(std::string & user, std::string & pw)
{
	if (!hash_password(pw)) {
		printf("Unable to hash password %s for account %s\n", pw.c_str(), user.c_str());
		return false;
	}
	
	if (pw != user_infos[user].user.account_password) {
		printf("Invalid password %s for account %s\n", pw.c_str(), user.c_str());
		return false;
	}
	
	return true;
}


bool sync_handler::hash_password(std::string &password)
{
	// start message digest
	const char *c_password = password.c_str();
	size_t c_password_len = strlen(c_password);
	unsigned char *digest = (unsigned char *)malloc(1024 * sizeof(char));
	unsigned int digest_len;
	
	EVP_MD_CTX *mdctx;
	
	if((mdctx = EVP_MD_CTX_create()) == NULL) {
		printf("EVP_MD_CTX_create error\n");
		return false;
	}
	
	if(1 != EVP_DigestInit_ex(mdctx, EVP_sha3_512(), NULL)) {
		printf("EVP_DigestInit_ex error\n");
		return false;
	}

	if(1 != EVP_DigestUpdate(mdctx, c_password, c_password_len)) {
		printf("EVP_DigestUpdate error\n");
		return false;
	}
	
	if((digest = (unsigned char *)OPENSSL_malloc(EVP_MD_size(EVP_sha3_512()))) == NULL) {
		printf("OPENSSL_malloc error\n");
		return false;
	}

	if(1 != EVP_DigestFinal_ex(mdctx, digest, &digest_len)) {
		printf("EVP_DigestFinal_ex error\n");
		return false;
	}
	
	EVP_MD_CTX_destroy(mdctx);
	// done message digest
	
	// start HEX encode
	static const char hexdig[] = "0123456789abcdef";
	size_t i;
	size_t j = 0;
	//size_t buflen = digest_len * 3;
	char *q = (char *)malloc((digest_len*2+1) * sizeof(char));
	const unsigned char *p;
	
	if (!q) {
		printf("Error: Unable to allocate for message digest to HEX\n");
		return false;
	}
	
	for (i = 0, p = digest; i < digest_len; i++, p++) {
        q[j++] = hexdig[(*p >> 4) & 0xf];
		q[j++] = hexdig[*p & 0xf];
	}

	q[j] = '\0';
	// done HEX encode
	
	password = q;
	
	free(digest);
	free(q);
	
	return true;
}
