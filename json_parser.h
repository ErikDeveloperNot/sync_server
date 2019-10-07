#ifndef _JSON_PARSER_H_
#define _JSON_PARSER_H_

#include "json_parser_exception.h"
#include "data_store_connection.h"

#include <string>
#include <vector>


struct register_config_request {
	std::string email;
	std::string password;
	std::string version;
};

struct register_config_reply {
	std::string bucket = "PassvaultServiceRegistration/service/sync-accounts";
	int port;
	std::string server;
	std::string protocol;
	std::string userName;
	std::string password;
	
	register_config_reply(const std::string &server, int port, std::string &protocol, std::string &user,
		std::string &pass) : 
		port{port},
		server{server},
		protocol{protocol},
		userName{user},
		password{pass} {}
		
	std::string serialize() {
		return "{\n\t\"bucket\":\"" + bucket + "\",\n\t\"password\":\"" + password + "\",\n" +
				"\t\"port\":" + std::to_string(port) + ",\n\t\"protocol\":\"" + protocol + "\",\n" +
				"\t\"server\":\"" + server + "\",\n\t\"userName\":\"" + userName + "\"\n}";
	}
};

struct sync_initial_account {
	std::string account_name;
	long long update_time;
	
	sync_initial_account() = default;
	sync_initial_account(std::string n, long long t) : account_name{n}, update_time{t} {}
};

struct sync_initial_request {
	register_config_request registerConfigReq;
	std::vector<sync_initial_account> accounts;
};

struct sync_initial_response {
	std::vector<Account> accountsToSendBackToClient;
	long long lockTime;
	int responseCode;
	std::vector<std::string> sendAccountsToServerList;
	
	std::string serialize() {
		std::string s = "{\n\t\"responseCode\":" + std::to_string(responseCode) + ",\n\t\"lockTime\":" +
				std::to_string(lockTime) + ",\n\t\"sendAccountsToServerList\":[";
				
		for (size_t i=0; i<sendAccountsToServerList.size(); i++) {
			s += "\"" + sendAccountsToServerList[i] + "\"";
			
			if (i+1 != sendAccountsToServerList.size())
				s += ",";
		}
		
		s += "],\n\t\"accountsToSendBackToClient\":[";
		
		for (size_t i=0; i<accountsToSendBackToClient.size(); i++) {
			s += "{\"accountName\":\"" + accountsToSendBackToClient[i].account_name + "\"," +
				"\"deleted\":" + ((accountsToSendBackToClient[i].deleted) ? "true" : "false") + "," +
				"\"password\":\"" + accountsToSendBackToClient[i].password + "\"," +
				"\"oldPassword\":\"" + accountsToSendBackToClient[i].old_password + "\"," +
				"\"updateTime\":" + std::to_string(accountsToSendBackToClient[i].update_time) + "," +
				"\"userName\":\"" + accountsToSendBackToClient[i].user_name + "\"," +
				"\"url\":\"" + accountsToSendBackToClient[i].url + "\"}";
				
				if (i+1 != accountsToSendBackToClient.size())
					s += ",";
		}
		
		s += "]\n}";
		
		return s;
	}
};

struct sync_final_request {
	long long lockTime;
	std::string password;
	std::string user;
	std::vector<Account> accounts;
};


/*
 * json parser declaration
 */ 
class json_parser
{
private:
	char invalid_json[24] = "Invalid json Exception\0";

	std::string get_next_json_object(std::string &);
	int eat_white_sapce(std::string &json, int index);
	int get_key(std::string &json, int index, std::string &key);
	int get_value(std::string &json, int index, std::string &value);
	int get_long_long_value(std::string &json, int index, long long &value);
	int get_bool_value(std::string &son, int index, bool &value);
	
	int get_start_of_object(std::string &json, int index);
	int verify_close_of_object(std::string &json, int index);
	
	register_config_request parse_user(std::string & request, bool register_config);
	
public:
	json_parser() = default;
	~json_parser() = default;
	
	register_config_request parse_register_config(std::string & request);
	register_config_request parse_delete_account(std::string & request);
	sync_initial_request parse_sync_initial(std::string & request);
	sync_final_request parse_sync_final(std::string & request);
//	std::string get_string_value(std::string &key, std::string object);

};

#endif // _JSON_PARSER_H_


