#include "sync_handler.h"
#include "jsonP_parser.h"
#include "register_server_exception.h"

#include "openssl/evp.h"

#include <thread>
#include <vector>
#include <map>
#include <cstring>


sync_handler::sync_handler(const std::string &thread_id, Config *config, 
							std::map<std::string, User_info> &user_infos, data_store_connection &store) :
t_id{thread_id},
store{store},
config{config},
user_infos{user_infos}
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
	jsonP_parser parser{};
	jsonP_doc *doc = nullptr;
	std::string email;
	std::string password;
	
	try {
		doc = parser.parse(request);
		email = doc->get_as_string("email");
		password = doc->get_as_string("password");
	} catch (jsonP_exception &ex) {
		printf("Error parsing register message: %s\n", ex.what());
		delete doc;
		throw register_server_exception{configHttp.build_reply(HTTP_400, close_con)};
	}
	
	delete doc;
	
	if (!verify_email(email)) {
		std::string error{"{ error: \"Invalid Email\" }"};
		throw register_server_exception{configHttp.build_reply(HTTP_400, close_con, error)};
	}
	
	//hash password
	std::string hashedPassword = password;
	
	if (!hash_password(hashedPassword)) {
		printf("Failed to hash password for user: %s\npass: %s\n", email.c_str(), password.c_str());
	}
	
	long long current_t = current_time_ms();
	//create the account
	User user{email, hashedPassword, 1L};
	User_info userInfo{user, current_t};
		
	//lock and check for account
	user_infos_lock.lock();
	
//debug_user_infos();	
	
	if (user_infos.count(email) < 1) {
		user_infos[user.account_uuid] = userInfo;
		user_infos_lock.unlock();
		user_infos_cv.notify_one();

		if (!store.createUser(user)) {
			//account creation failed
			user_infos_lock.lock();
			user_infos.erase(user.account_uuid);
			user_infos_lock.unlock();
			user_infos_cv.notify_one();
			throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
		} /*else {
			// unlock user
			unlock_user(user.account_uuid, current_t);
		}*/
	} else {
		//user already exist return
		user_infos_lock.unlock();
		user_infos_cv.notify_one();
		throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
	}
	
	std::string prot{"https"};
	jsonP_doc d{};
	d.add_element("bucket", new element_string{"PassvaultServiceRegistration/service/sync-accounts"});
	d.add_element("port", new element_numeric{config->getBindPort()});
	d.add_element("protocol", new element_string{prot});
	d.add_element("userName", new element_string{email});
	d.add_element("password", new element_string{password});
	d.add_element("server", new element_string{config->getBindAddress()});
	std::string s;
	d.stringify(s);
	
	return s;
}


std::string sync_handler::handle_config()
{
	jsonP_doc doc{};
	doc.add_element("bucket", new element_string{"PassvaultServiceRegistration/service/sync-accounts"});
	doc.add_element("password", new element_string{"xxx"});
	doc.add_element("port", new element_numeric{config->getBindPort()});
	doc.add_element("protocol", new element_string{"https"});
	doc.add_element("server", new element_string{config->getBindAddress()});
	doc.add_element("userName", new element_string{"xxx@xxx.com"});
	std::string s;
	doc.stringify(s);

	return s;
}


std::string sync_handler::handle_delete(std::string& request)
{
//	printf("request:\n%s\n", request.c_str());

	jsonP_parser parser{};
	jsonP_doc *doc = nullptr;
	std::string email;
	std::string password;
	
	try {
		doc = parser.parse(request);
		email = doc->get_as_string("user");
		password = doc->get_as_string("password");
	} catch (jsonP_exception &ex) {
		printf("Error parsing delete message: %s\n", ex.what());
		delete doc;
		throw register_server_exception{configHttp.build_reply(HTTP_400, close_con)};
	}

	delete doc;
//	long long current_t = lock_user(registerConfigReq.email);
	
	if (!verify_password(email, password)) {
//		unlock_user(registerConfigReq.email, current_t);
		throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
	}
	
	if (!store.delete_user(user_infos[email].user)) {
		printf("Error deleting account %s from the database\n", email.c_str());
//		unlock_user(registerConfigReq.email, current_t);
		throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
	}
	
	user_infos_lock.lock();
//	user_infos.erase(registerConfigReq.email);
	user_infos.erase(email);
	user_infos_lock.unlock();
	user_infos_cv.notify_one();
	
	return "{ msg: \"Account has been deleted\" }";
}


std::string sync_handler::handle_sync_initial(std::string& request)
{
//	printf("Sync Initial:\n%s\n", request.c_str());

	jsonP_parser parser{};
	jsonP_doc *doc = nullptr;
	jsonP_doc *resp_doc = nullptr;
	element_array *sendAccountsToServerList = nullptr;
	element_array *accountsToSendBackToClient = nullptr;
	std::string email;
	std::string password;
	
	try {
		doc = parser.parse(request);
		email = doc->get_as_string("user");
		password = doc->get_as_string("password");
		
		if (!verify_password(email, password)) {
			delete doc;
			throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
		}
		
		sendAccountsToServerList = new element_array{string};
		accountsToSendBackToClient = new element_array{object};
		resp_doc = new jsonP_doc{};
		resp_doc->add_element("lockTime", new element_numeric{current_time_sec()});
		resp_doc->add_element("responseCode", new element_numeric{0});

		std::map<std::string, Account> accounts = store.get_accounts_for_user(email);
		std::string account_name;
		
		/*
		 * for each account sent from client check with store list to see if send back/send to/nothing
		 */
		for (element *e : doc->get_as_array("accounts")) {
			account_name = e->get_as_string("accountName");
			if (accounts.count(account_name) > 0) {
				if (e->get_as_numeric_long("updateTime") > accounts[account_name].update_time) {
					//client version more recent - add to client send back to server
					sendAccountsToServerList->add_element(new element_string{account_name});
				} else if (accounts[account_name].update_time > e->get_as_numeric_long("updateTime")) {
					//server version more recent - add to send back to client
					element_object *obj = new element_object{};
					obj->add_element("accountName", new element_string{account_name});
					obj->add_element("deleted", new element_boolean{accounts[account_name].deleted});
					obj->add_element("password", new element_string{accounts[account_name].password});
					obj->add_element("oldPassword", new element_string{accounts[account_name].old_password});
					obj->add_element("updateTime", new element_numeric{long(accounts[account_name].update_time)});
					obj->add_element("userName", new element_string{accounts[account_name].user_name});
					obj->add_element("url", new element_string{accounts[account_name].url});
					accountsToSendBackToClient->add_element(obj);
				}
				
				accounts.erase(account_name);
			} else {
				//no version exists on server - add to client send back to server
				sendAccountsToServerList->add_element(new element_string{account_name});
			}
		}
		
		//whatever is left in map should be sent back to client
		for (auto &a : accounts) {
			element_object *obj = new element_object{};
			obj->add_element("accountName", new element_string{a.second.account_name});
			obj->add_element("deleted", new element_boolean{a.second.deleted});
			obj->add_element("password", new element_string{a.second.password});
			obj->add_element("oldPassword", new element_string{a.second.old_password});
			obj->add_element("updateTime", new element_numeric{long(a.second.update_time)});
			obj->add_element("userName", new element_string{a.second.user_name});
			obj->add_element("url", new element_string{a.second.url});
			accountsToSendBackToClient->add_element(obj);
		}
	
		resp_doc->add_element("sendAccountsToServerList", sendAccountsToServerList);
		resp_doc->add_element("accountsToSendBackToClient", accountsToSendBackToClient);
		std::string s;
		resp_doc->stringify(s);

		delete doc;
		delete resp_doc;
		return s;
		
	} catch (jsonP_exception &ex) {
		printf("Error parsing sync initial message: %s\n", ex.what());
		delete doc;
		delete resp_doc;
		throw register_server_exception{configHttp.build_reply(HTTP_400, close_con)};
	}
}


std::string sync_handler::handle_sync_final(std::string& request)
{
//	printf("SYNC FINAL:\n%s\n", request.c_str());
	jsonP_parser parser{};
	jsonP_doc *doc = nullptr;
	std::string user;
	std::vector<Account> accounts;
	
	try {
		doc = parser.parse(request);
		user = doc->get_as_string("user");
		
		if (!verify_password(user, doc->get_as_string("password"))) {
			delete doc;
			throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
		}
		
		for (element *obj : doc->get_as_array("accounts")) {
			accounts.emplace_back(obj->get_as_string("accountName"), user, obj->get_as_string("userName"),
				obj->get_as_string("password"), obj->get_as_string("oldPassword"), obj->get_as_string("url"),
				obj->get_as_numeric_long("updateTime"), obj->get_as_boolean("deleted"));
		}
		
		delete doc;
		
	} catch (jsonP_exception &ex) {
		delete doc;
		printf("Parse error in sync final message: %s\n", ex.what());
		throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
	}
	
	long long relockTime = current_time_sec();
	
	//update store
	if (store.upsert_accounts_for_user(user, accounts)) {
		if (!store.update_last_sync_for_user(user, relockTime)) {
			printf("Failed to update last SyncTime for user: %s in syncFinal\n", user.c_str());
			throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
		}
	} else {
		//means there was at least 1 failure, so dont update last sync time so another sync can be done
		printf("Failed to update accounts for user: %s in syncFinal\n", user.c_str());
		throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
	}

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
//		auto sec = std::chrono::seconds(30);
		
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
			for (; it != user_infos[forUser].cv_vector.end(); it++)
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


//will not allow quoted labels or bracketed domains or ip domains
bool sync_handler::verify_email(std::string& email)
{
	bool local_valid{false};
	int local_l_max{64};
	char last_char = '.';
	int i{0};

	for ( ; i < email.length(); i++) {
		char c = email[i];
		
		if (c == '@') {
			if (last_char == '.')
				return false;
				
			break;
		} else if ((c == '.' && last_char == '.') || i >= local_l_max) {
			return false;
		} else if ((c >= ']' && c <= '~') || (c >= '?' && c <= '[') || c == '!' || c == '=' ||
					(c >= '#' && c <= '\'') || (c >= '*' && c<= '+') || (c >= '-' && c <= '9')) {
							
			local_valid = true;
			last_char = c;
		} else {
			return false;
		}
					
	}
	
	if (!local_valid)
		return false;
		
	i++;
	bool all_alpha{true};
	bool domain_valid{false};
	int domain_length{0};
	int label_length{0};
	int domain_l_max{253};
	int label_l_max{63};
	
	last_char = '.';

	for ( ; i < email.length(); i++, domain_length++, label_length++) {
		char c = email[i];

		if (domain_length > domain_l_max || label_length > label_l_max || c == '@')
			return false;
	
		if (c == '.') {
			if (last_char == '.' || last_char == '-')
				return false;
				
			label_length = 0;
			domain_valid = false;
			all_alpha = true;
		} else if (c == '-') {
			if (last_char == '.' || last_char == '-')
				return false;
				
			domain_valid = false;
			all_alpha = false;
		} else if ((c >= ']' && c <= '~') || (c >= '?' && c <= '[') || c == '!' || c == '=' ||
					(c >= '#' && c <= '\'') || (c >= '*' && c<= '+') || (c >= '-' && c <= '9')) {
			
			domain_valid = true;
						
			if ((c < 'a' || c > 'z') && (c < 'A' || c > 'Z'))
				all_alpha = false;
		} else {
			return false;
		}
		
		last_char = c;
	}

	if (domain_valid && all_alpha)
		return true;
		
	return false;
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
