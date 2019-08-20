#include "sync_handler.h"
#include "json_parser.h"
#include "register_server_exception.h"

#include "openssl/evp.h"

#include <thread>
#include <vector>
#include <map>
#include <cstring>




sync_handler::sync_handler(const std::string &thread_id, Config *config, 
							std::map<std::string, User_info> &user_infos) :
t_id{thread_id},
config{config},
user_infos{user_infos}
{
	bool success{false};
	
	do {
		success = store.initialize(t_id, config);
		
		if (!success) {
			printf("%s: error initializing data store, will try again\n");
			std::this_thread::sleep_for(std::chrono::milliseconds(20000));
		}
	} while (!success);
	
	std::call_once(onceFlag, [&]() {
		printf("%s: initializing the user_infos set\n", t_id.c_str());
		std::vector<User> users = store.getAllUsers();
		
		for (User x : users) {
			user_infos[x.account_uuid] = User_info{x, 0L};
		}
		
		debug_user_infos();
//		for (auto &k : user_infos) {
//			printf("%s, %d\n", k.first.c_str(), user_infos[k.first].lock_time);
//		}
	});
	
}

sync_handler::~sync_handler()
{
}


std::string sync_handler::handle_request(std::string &resource, std::string &request, request_type http_type)
{
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
		return STATUS_400;
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
		throw register_server_exception{STATUS_400};
	}
	
	//hash password
	std::string hashedPassword = registerConfigReq.password;
	
	if (!hash_password(hashedPassword)) {
		printf("Failed to hash password for user: %s\npass: %s\n", registerConfigReq.email.c_str(),
				registerConfigReq.password.c_str());
	}
	
	//lock and check for account
	user_infos_lock.lock();
//debug_user_infos();	
	
	if (user_infos.count(registerConfigReq.email) < 1) {
		//create the account
		User user{registerConfigReq.email, hashedPassword, 1L};
		User_info userInfo{user, current_time_ms()};
		user_infos[user.account_uuid] = userInfo;
		user_infos_lock.unlock();
		user_infos_cv.notify_one();

		if (!store.createUser(user)) {
			//account creation failed
			user_infos_lock.lock();
			user_infos.erase(user.account_uuid);
			user_infos_lock.unlock();
			user_infos_cv.notify_one();
			throw register_server_exception{STATUS_500_SERVER_ERROR};
		} else {
			// unlock user
			unlock_user(user.account_uuid);
		}
	} else {
		//user already exist return
		user_infos_lock.unlock();
		user_infos_cv.notify_one();
		throw register_server_exception{STATUS_500_USER_EXISTS};
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
		throw register_server_exception{STATUS_400};
	}

	lock_user(registerConfigReq.email);
	
	std::string hashedPass = registerConfigReq.password;
	if (!hash_password(hashedPass)) {
		printf("Unable to hash password %s for account %s\n", registerConfigReq.password.c_str(), registerConfigReq.email.c_str());
		unlock_user(registerConfigReq.email);
		throw register_server_exception{STATUS_401};
	}
	
	if (hashedPass != user_infos[registerConfigReq.email].user.account_password) {
		printf("Invalid password %s for account %s\n", hashedPass.c_str(), registerConfigReq.email.c_str());
		unlock_user(registerConfigReq.email);
		throw register_server_exception{STATUS_401};
	}
	
	if (!store.delete_user(user_infos[registerConfigReq.email].user)) {
		printf("Error deleting account %s from the database\n", registerConfigReq.email.c_str());
		unlock_user(registerConfigReq.email);
		throw register_server_exception{STATUS_500_SERVER_ERROR};
	}
	
	user_infos_lock.lock();
	user_infos.erase(registerConfigReq.email);
	user_infos_lock.unlock();
	user_infos_cv.notify_one();
	
	return "Account has been deleted.";
}


std::string sync_handler::handle_sync_initial(std::string& request)
{
//	printf("%s\n", request.c_str());
	json_parser parser{};
	sync_initial_request syncReq;
	
	try {
		syncReq = parser.parse_sync_initial(request);
	} catch (json_parser_exception &ex) {
		throw register_server_exception{STATUS_400};
	}

	sync_initial_response syncResp;
	syncResp.lockTime = lock_user(syncReq.registerConfigReq.email);
	syncResp.responseCode = 0; //TODO TODO -  check on this
	
	if (!hash_password(syncReq.registerConfigReq.password)  || 
		syncReq.registerConfigReq.password != user_infos[syncReq.registerConfigReq.email].user.account_password) {
			
		printf("Invalid password sent for user: %s, sent: %s, expected %s\n", syncReq.registerConfigReq.email.c_str(),
					syncReq.registerConfigReq.password.c_str(), 
					user_infos[syncReq.registerConfigReq.email].user.account_password.c_str());
					
		unlock_user(syncReq.registerConfigReq.email);
		throw register_server_exception{STATUS_401};
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
		throw register_server_exception{STATUS_400};
	}
	
	long long relockTime = relock_user(syncFinal.user, syncFinal.lockTime);

	//update store
	if (store.upsert_accounts_for_user(syncFinal.user, syncFinal.accounts)) {
		if (!store.update_last_sync_for_user(syncFinal.user, relockTime)) {
			printf("Failed to update last SyncTime for user: %s in syncFinal\n", syncFinal.user.c_str());
			unlock_user(syncFinal.user);
			throw register_server_exception{STATUS_500_SERVER_ERROR};
		}
	} else {
		//means there was at least 1 failure, so dont update last sync time so another sync can be done
		printf("Failed to update accounts for user: %s in syncFinal\n", syncFinal.user.c_str());
		unlock_user(syncFinal.user);
		throw register_server_exception{STATUS_500_SERVER_ERROR};
	}
	
	//unlock user
	unlock_user(syncFinal.user);
	
	return "Sync Final Complete";
}


long long sync_handler::current_time_ms()
{
//	printf("%ld\n", std::chrono::system_clock::now().time_since_epoch().count()/1000);
	return std::chrono::system_clock::now().time_since_epoch().count()/1000;
}


long long sync_handler::lock_user(std::string& forUser)
{
	std::unique_lock<std::mutex> lock(user_infos_lock);
	
	if (user_infos.count(forUser) < 1) {
		lock.unlock();
		user_infos_cv.notify_one();

		throw register_server_exception{STATUS_401};
	}
	
	long long current_lock = current_time_ms();

	if (current_lock - user_infos[forUser].lock_time < LOCK_TIMEOUT) {
		auto sec = std::chrono::seconds((current_lock - user_infos[forUser].lock_time) / 1000 + 1);
		
		while (!user_infos_cv.wait_for(lock, sec, [&]() {
			current_lock = current_time_ms();
			printf("check if %lld - %lld > %lld\n", current_lock, user_infos[forUser].lock_time, sec);
			return (current_lock - user_infos[forUser].lock_time > LOCK_TIMEOUT);
		})) {}
		
		//if still locked throw 503
		current_lock = current_time_ms();
		if (current_lock - user_infos[forUser].lock_time < LOCK_TIMEOUT) {
			lock.unlock();
			user_infos_cv.notify_one();
			throw register_server_exception{STATUS_503};
		}
	}
	
	user_infos[forUser].lock_time = current_lock;
	lock.unlock();
	user_infos_cv.notify_one();
	
	return current_lock;
}


long long sync_handler::relock_user(std::string& forUser, long long userLock)
{
	long long toReturn{0};
	std::unique_lock<std::mutex> lock(user_infos_lock);
	
printf("forUser lock: %lld, user_infos lock: %lld\n", userLock, user_infos[forUser].lock_time);
	
	if (user_infos.count(forUser) < 1) {
		lock.unlock();
		user_infos_cv.notify_one();

		throw register_server_exception{STATUS_401};
	}
	
	if (userLock != user_infos[forUser].lock_time) {
		//slow client fail
		throw register_server_exception{STATUS_401};
	} else {
		toReturn = current_time_ms();
		user_infos[forUser].lock_time = toReturn;
	}
	
	lock.unlock();
	user_infos_cv.notify_one();
	
	return toReturn;
}


void sync_handler::unlock_user(std::string& forUser)
{
	printf("b4 unlock\n");
	debug_user_infos();
	
	user_infos_lock.lock();
	user_infos[forUser].lock_time = 0;
	user_infos_lock.unlock();
	user_infos_cv.notify_one();
	
	printf("after unlock\n");
	debug_user_infos();
}


void sync_handler::debug_user_infos()
{
	for (auto &k : user_infos) {
		printf("%s, %lld\n", k.first.c_str(), user_infos[k.first].lock_time);
	}
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