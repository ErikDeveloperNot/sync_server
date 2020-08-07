#include "sync_handler.h"
#include "register_server_exception.h"

#include "openssl/evp.h"

#include <thread>
#include <vector>
#include <map>
#include <cstring>


sync_handler::sync_handler(const std::string &thread_id, Config *config, 
							std::map<char *, User_info, cmp_key> &user_infos, IDataStore *store) :
t_id{thread_id},
store{store},
config{config},
user_infos{user_infos}
{
	std::call_once(onceFlag, [&]() {
		printf("%s: initializing the user_infos set\n", t_id.c_str());
		std::vector<User> users = store->getAllUsers();
		
		for (User x : users) {
			user_infos[x.account_uuid] = User_info{x, 0L};
		}
		
		debug_user_infos();
	});
	
}

sync_handler::~sync_handler()
{
}


char * sync_handler::handle_request(operation_type op_type, char *request, request_type http_type)
{
	try {
		if (op_type == register_config) {
			if (http_type == request_type::POST) {
				return handle_register(request);
			} else {
				return handle_config();
			}
		} else if (op_type == delete_user) {
			return handle_delete(request);
		} else if (op_type == sync_initial) {
			return handle_sync_initial(request);
		} else if (op_type == sync_final) {
			return handle_sync_final(request);
		} else {
			return bad_request;
		}
	} catch (const char *error) {
		throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
	}
}


char * sync_handler::handle_register(char *request)
{
//	printf("request:\n%s\n", request.c_str());
	jsonP_parser parser{request, (unsigned int)strlen(request), PRESERVE_JSON};  // <-- REMOVE PRESERVE LATER
	jsonP_json *pDoc = nullptr;
	const char *email;
	const char *password;
	
	try {
		pDoc = parser.parse();
		email = pDoc->get_string_value(email_path, delim, &jsonP_err);
		password = pDoc->get_string_value(pass_path, delim, &jsonP_err);
	} catch (jsonP_exception &ex) {
		printf("Error parsing register message: %s\n", ex.what());
		delete pDoc;
		throw register_server_exception{configHttp.build_reply(HTTP_400, close_con)};
	}
	
	if (!verify_email(email)) {
		char error[] = {"{ error: \"Invalid Email\" }"};
		throw register_server_exception{configHttp.build_reply(HTTP_400, close_con, error)};
	}
	
	//hash password
	char hashedPassword[1024];

	if (!hash_password(password, hashedPassword)) {
		printf("Failed to hash password for user: %s\npass: %s\n", email, password);
	}
	
	long long current_t = current_time_ms();
	//create the account
	User user{email, hashedPassword, 1L};
		
	//lock and check for account
	user_infos_lock.lock();
	
//debug_user_infos();	
	
	if (user_infos.count((char*)email) < 1) {
		user_infos[user.account_uuid] = User_info{user, current_t};
		user_infos_lock.unlock();
		user_infos_cv.notify_one();

		if (!store->createUser(user)) {
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
	
	long p = config->getBindPort();
	jsonP_json d{object, 6, 512, DONT_SORT_KEYS};
	d.add_value_type(string, 0, bucket_key, sync_path);
	d.add_value_type(numeric_long, 0, port_key, &p);
	d.add_value_type(string, 0, protocol_key, proto);
	d.add_value_type(string, 0, user_key, (char*)email);
	d.add_value_type(string, 0, pass_key, (char*)password);
	d.add_value_type(string, 0, server_key, (char*)config->getBindAddress());

	delete pDoc;
	
	return d.stringify();
}


char * sync_handler::handle_config()
{
	long p = config->getBindPort();
	char email[] = {"xxx@xxx.com"};
	char password[] = {"xxx"};
	jsonP_json d{object, 6, 512, DONT_SORT_KEYS};
	d.add_value_type(string, 0, bucket_key, sync_path);
	d.add_value_type(numeric_long, 0, port_key, &p);
	d.add_value_type(string, 0, protocol_key, proto);
	d.add_value_type(string, 0, user_key, email);
	d.add_value_type(string, 0, pass_key, password);
	d.add_value_type(string, 0, server_key, (char*)config->getBindAddress());

	return d.stringify();
}


char * sync_handler::handle_delete(char *request)
{
//	printf("request:\n%s\n", request.c_str());
	jsonP_parser parser{request, (unsigned int)strlen(request), PRESERVE_JSON};  // <-- REMOVE PRESERVE LATER
	jsonP_json *pDoc = nullptr;
	const char *email;
	const char *password;
	
	try {
		pDoc = parser.parse();
		email = pDoc->get_string_value(user_path, delim, &jsonP_err);
		password = pDoc->get_string_value(pass_path, delim, &jsonP_err);

//printf("**** EMAIL: %s, PASSWORD: %s\n", email, password);
	} catch (jsonP_exception &ex) {
		printf("Error parsing delete message: %s\n", ex.what());
		delete pDoc;
		throw register_server_exception{configHttp.build_reply(HTTP_400, close_con)};
	}

	
//	long long current_t = lock_user(registerConfigReq.email);
	
	if (!verify_password(email, password)) {
//		unlock_user(registerConfigReq.email, current_t);
		throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
	}
	
	if (!store->delete_user(user_infos[(char*)email].user)) {
		printf("Error deleting account %s from the database\n", email);
//		unlock_user(registerConfigReq.email, current_t);
		throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
	}
	
	user_infos_lock.lock();
	char *id = user_infos[(char*)email].user.account_uuid;
	char *pass = user_infos[(char*)email].user.account_password;
	user_infos.erase((char*)email);
	free(id);
	free(pass);	
	user_infos_lock.unlock();
	user_infos_cv.notify_one();
	
	delete pDoc;
	
	return account_deleted;
}


char * sync_handler::handle_sync_initial(char *request)
{
//	printf("Sync Initial:\n%s\n", request);

	jsonP_parser parser{request, (unsigned int)strlen(request), PRESERVE_JSON };    //<--- REMOVE LATER
	jsonP_json *pDoc = nullptr;
//	char *heap = NULL;
	Heap_List heap{};
	const char *email;
	const char *password;
	
	try {
		pDoc = parser.parse();
		email = pDoc->get_string_value(user_path, delim, &jsonP_err);
		password = pDoc->get_string_value(pass_path, delim, &jsonP_err);

		if (!verify_password(email, password)) {
			delete pDoc;
			throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
		}
		
		long long cts = current_time_sec();
		jsonP_json resp_doc{object, 4, 1024, DONT_SORT_KEYS};
		resp_doc.add_value_type(numeric_long, 0, responseCode_key, &default_response_code);
		resp_doc.add_value_type(numeric_long, 0, lockTime_key, &cts);
		std::map<char*, Account, cmp_key> accounts = store->get_accounts_for_user(email, &heap);
		char *account_name;
		
		object_id sendAccountsToServerList = resp_doc.add_container(sendAccountsToServerList_key, 5, 0, array);
		object_id accountsToSendBackToClient = resp_doc.add_container(accountsToSendBackToClient_key, 5, 0, array);
		
		unsigned int mem_cnt = pDoc->get_elements_count(accounts_key, delim);
		const char acct_name_src[] = {"/accounts/%d/accountName"};
		const char update_time_src[] = {"/accounts/%d/updateTime"};
		char p_dst[100];
		
		/*
		 * for each account sent from client check with store list to see if send back/send to/nothing
		 */
		for (int i=0; i < mem_cnt; i++) {
			sprintf(p_dst, acct_name_src, i);
			account_name = (char*) pDoc->get_string_value(p_dst, delim, &jsonP_err);

//fprintf(stderr, "\n\nACCOUNT_NAME: %s  -  %s\n\n", account_name, p_dst);
//if (jsonP_err != none) {
//	char *t = pDoc->stringify_pretty();
//	fprintf(stderr, "ERROR:%d-%d-%d\n%s\n\n", jsonP_err, i, mem_cnt, t);
//	free(t);
////	continue;
//}

			if (accounts.count(account_name) > 0) {
				sprintf(p_dst, update_time_src, i);
				long update_time = pDoc->get_long_value(p_dst, delim, &jsonP_err);

				if (update_time > accounts[account_name].update_time) {
					//client version more recent - add to client send back to server
					resp_doc.add_value_type(string, sendAccountsToServerList, NULL, account_name);
				} else if (update_time < accounts[account_name].update_time) {
					//server version more recent - add to send back to client
					object_id mem_id = resp_doc.add_container(NULL, 7, accountsToSendBackToClient, object);
					resp_doc.add_value_type(string, mem_id, accountName_key, account_name);
					resp_doc.add_value_type(((accounts[account_name].deleted) ? bool_true : bool_false), mem_id, deleted_key, NULL);
					resp_doc.add_value_type(string, mem_id, pass_key, accounts[account_name].password);
					resp_doc.add_value_type(string, mem_id, oldPass_key, accounts[account_name].old_password);
					resp_doc.add_value_type(numeric_long, mem_id, updateTime_key, &accounts[account_name].update_time);
					resp_doc.add_value_type(string, mem_id, user_key, accounts[account_name].user_name);
					resp_doc.add_value_type(string, mem_id, url_key, accounts[account_name].url);
				}
				
				accounts.erase(account_name);
			} else {
				//no version exists on server - add to client send back to server
				resp_doc.add_value_type(string, sendAccountsToServerList, NULL, account_name);
			}
		}
		
		//whatever is left in map should be sent back to client
		for (auto &a : accounts) { 
			object_id mem_id = resp_doc.add_container(NULL, 7, accountsToSendBackToClient, object);
			resp_doc.add_value_type(string, mem_id, accountName_key, a.second.account_name);
			resp_doc.add_value_type(((a.second.deleted) ? bool_true : bool_false), mem_id, deleted_key, NULL);
			resp_doc.add_value_type(string, mem_id, pass_key, a.second.password);
			resp_doc.add_value_type(string, mem_id, oldPass_key, a.second.old_password);
			resp_doc.add_value_type(numeric_long, mem_id, updateTime_key, &a.second.update_time);
			resp_doc.add_value_type(string, mem_id, user_key, a.second.user_name);
			resp_doc.add_value_type(string, mem_id, url_key, a.second.url);
		}
	
		delete pDoc;
		
		return resp_doc.stringify();
	} catch (jsonP_exception &ex) {
		printf("Error parsing sync initial message: %s\n", ex.what());
		delete pDoc;
		throw register_server_exception{configHttp.build_reply(HTTP_400, close_con)};
	}
}


char * sync_handler::handle_sync_final(char *request)
{
//	printf("SYNC FINAL:\n%s\n", request.c_str());
	jsonP_parser parser{request, (unsigned int)strlen(request), PRESERVE_JSON};    //<--- REMOVE LATER
	jsonP_json *pDoc = nullptr;
	
	std::vector<Account> accounts;
	const char *email;
	const char *password;
	
	try {
		pDoc = parser.parse();
		email = pDoc->get_string_value(user_path, delim, &jsonP_err);
		password = pDoc->get_string_value(pass_path, delim, &jsonP_err);

		if (!verify_password(email, password)) {
			delete pDoc;
			throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
		}
		
		const void *value;
		object_id id = pDoc->get_object_id(accounts_path, delim);
		element_type typ = pDoc->get_next_array_element(id, value);
		
		while (typ == object) {
			id = *(object_id*)value;
			accounts.emplace_back(
				(char*)pDoc->get_string_value(accountName_key, id, &jsonP_err),
				(char*)email,
				(char*)pDoc->get_string_value(user_key, id, &jsonP_err),
				(char*)pDoc->get_string_value(pass_key, id, &jsonP_err),
				(char*)pDoc->get_string_value(oldPass_key, id, &jsonP_err),
				(char*)pDoc->get_string_value(url_key, id, &jsonP_err),
				pDoc->get_long_value(updateTime_key, id, &jsonP_err),
				pDoc->get_bool_value(deleted_key, id, &jsonP_err)
			);
			
			typ = pDoc->get_next_array_element(0, value);
		}
		
	} catch (jsonP_exception &ex) {
		delete pDoc;
		printf("Parse error in sync final message: %s\n", ex.what());
		throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
	}
	
	long relockTime = (long)current_time_sec();
	//update store
	if (store->upsert_accounts_for_user((char*)email, accounts)) {
		if (!store->update_last_sync_for_user(email, relockTime)) {
			printf("Failed to update last SyncTime for user: %s in syncFinal\n", email);
printf(">>>>>>>>> relockTime: %ld\n", relockTime);
			delete pDoc;
			throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
		}
	} else {
		//means there was at least 1 failure, so dont update last sync time so another sync can be done
		printf("Failed to update accounts for user: %s in syncFinal\n", email);
		delete pDoc;
		throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
	}

	delete pDoc;
	
//	char *resp = (char*)malloc(strlen(sync_final_resp)+1);
//	strcpy(resp, sync_final_resp);

	return sync_final_resp;
}


long long sync_handler::current_time_ms()
{
//	printf("-----------%lld\n", std::chrono::system_clock::now().time_since_epoch().count()/1000);
	return std::chrono::system_clock::now().time_since_epoch().count()/1000;
}


long long sync_handler::current_time_sec()
{
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}


/*
 *  deprecated -  no longer needed, instead use upsert with on conflict
 */
long long sync_handler::lock_user(const char *forUser)
{
	std::unique_lock<std::mutex> lock(user_infos_lock);
	
	if (user_infos.count((char*)forUser) < 1) {
		lock.unlock();
		user_infos_cv.notify_one();
		throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
	}
	
	if (!user_infos[(char*)forUser].cv_vector.empty()) {
		printf("%s - waiting for lock for user: %s\n", t_id.c_str(), forUser);
//		auto sec = std::chrono::seconds(30);
		
		// add hndler/thread cond variable to user queue, release user_info_lock and wait on cv. TODO - add timeout later
		std::unique_lock<std::mutex> temp_lock(handler_mutex);
		user_infos[(char*)forUser].cv_vector.push_back(&handler_cv);
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
		printf("%s - notified waiting for %s\n", t_id.c_str(), forUser);
		lock.lock();
		
		//should never see this
		if (user_infos[(char*)forUser].cv_vector[0] != &handler_cv) {
			printf("%s - %s notified but wrong cv is in front\n", t_id.c_str(), forUser);
			
			std::vector<std::condition_variable *>::iterator it = user_infos[(char*)forUser].cv_vector.begin();
			for (; it != user_infos[(char*)forUser].cv_vector.end(); it++)
				if (*it == &handler_cv)
					break;
					
			if (it != user_infos[(char*)forUser].cv_vector.end())
				user_infos[(char*)forUser].cv_vector.erase(it);
			
			lock.unlock();
			user_infos_cv.notify_one();
			
			throw register_server_exception{configHttp.build_reply(HTTP_500, close_con)};
		}

//		if (handler_cv.wait_for(temp_lock, sec) == std::cv_status::timeout) {
//			printf("%s - timed out waiting for lock for user: %s\n", t_id.c_str(), forUser.c_str());
//			throw register_server_exception{configHttp.build_reply(HTTP_503, close_con)};
//		}
	} else {
		user_infos[(char*)forUser].cv_vector.push_back(&handler_cv);
	}



	long long current_lock = current_time_sec();

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
	
	user_infos[(char*)forUser].lock_time = current_lock;
	lock.unlock();
	user_infos_cv.notify_one();
	
	return current_lock;
}


/*
 *  deprecated -  no longer needed, instead use upsert with on conflict
 */
long long sync_handler::relock_user(const char *forUser, long long userLock)
{
	long long toReturn{0};
	std::unique_lock<std::mutex> lock(user_infos_lock);
	
	if (user_infos.count((char*)forUser) < 1) {
		lock.unlock();
		user_infos_cv.notify_one();

//		throw register_server_exception{STATUS_401};
		throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
	}
	
	if (userLock != user_infos[(char*)forUser].lock_time) {
		//slow client or server, fail
		throw register_server_exception{configHttp.build_reply(HTTP_403, close_con)};
	} else {
		toReturn = current_time_sec();
		user_infos[(char*)forUser].lock_time = toReturn;
	}
	
	lock.unlock();
	user_infos_cv.notify_one();
	
	return toReturn;
}

/*
 *  deprecated -  no longer needed, instead use upsert with on conflict
 */
void sync_handler::unlock_user(const char *forUser, long long lockTime)
{
	user_infos_lock.lock();
//	user_infos[forUser].lock_time = 0;
	
	if (lockTime != user_infos[(char*)forUser].lock_time) {
		printf("%s - Tried to unlock for user: %s, but locktimes dont match\n", t_id.c_str(), forUser);
	} else {
		user_infos[(char*)forUser].lock_time = 0;
	
	
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
		
		if (!user_infos[(char*)forUser].cv_vector.empty()) {
			user_infos[(char*)forUser].cv_vector.erase(user_infos[(char*)forUser].cv_vector.begin());
			
			if (!user_infos[(char*)forUser].cv_vector.empty()) 
				user_infos[(char*)forUser].cv_vector[0]->notify_one();
		}
	}
	
	user_infos_lock.unlock();
	user_infos_cv.notify_one();
}


void sync_handler::debug_user_infos()
{
	for (auto &k : user_infos) {
		printf("%s, last sync: %lld\n", k.first, user_infos[k.first].user.account_last_sync);
	}
}


//bool sync_handler::verify_password(std::string & user, std::string & pw)
bool sync_handler::verify_password(const char *user, const char *pw)
{
	if (user_infos.count((char*)user) < 1)
		return false;
		
	char hashed[1024];
	
	if (!hash_password(pw, hashed)) {
		printf("Unable to hash password %s for account %s\n", pw, user);
		return false;
	}
	if (strcmp(user_infos[(char*)user].user.account_password, hashed) != 0) {
		printf("Invalid password %s for account %s\n", pw, user);
		return false;
	}
	
	return true;
}


//will not allow quoted labels or bracketed domains or ip domains
bool sync_handler::verify_email(const char *email) //std::string& email)
{
	size_t email_len = strlen(email);
	bool local_valid{false};
	int local_l_max{64};
	char last_char = '.';
	int i{0};

//	for ( ; i < email.length(); i++) {
	for ( ; i < email_len; i++) {
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

//	for ( ; i < email.length(); i++, domain_length++, label_length++) {
	for ( ; i < email_len; i++, domain_length++, label_length++) {
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


bool sync_handler::hash_password(const char *password, char *hashed)
{
	// start message digest
	size_t c_password_len = strlen(password);
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

	if(1 != EVP_DigestUpdate(mdctx, password, c_password_len)) {
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
//	char *q = (char *)malloc((digest_len*2+1) * sizeof(char));
	const unsigned char *p;
	
	
	for (i = 0, p = digest; i < digest_len; i++, p++) {
        hashed[j++] = hexdig[(*p >> 4) & 0xf];
		hashed[j++] = hexdig[*p & 0xf];
	}

	hashed[j] = '\0';
	// done HEX encode
	
	free(digest);
	
	return true;
}
