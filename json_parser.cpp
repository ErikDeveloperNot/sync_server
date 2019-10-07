#include "json_parser.h"



register_config_request json_parser::parse_register_config(std::string& json)
{
	return parse_user(json, true);
}


register_config_request json_parser::parse_delete_account(std::string& json)
{
	return parse_user(json, false);
}


register_config_request json_parser::parse_user(std::string& json, bool register_config)
{
	int index = get_start_of_object(json, 0);
	
	register_config_request registerConfigReq;
	std::string key{};
	std::string value{};
	bool email{false}, pass{false}, version{false};
	
	do {
		index = get_key(json, index, key);
//		printf("index: %d, key: %s\n", index, key.c_str());
		
		if (key == "email" && register_config) {
			index = get_value(json, index, registerConfigReq.email);
			email = true;
		} else if (key == "user" && !register_config) {
			index = get_value(json, index, registerConfigReq.email);
			email = true;
			version = true;
		} else if (key == "password") {
			index = get_value(json, index, registerConfigReq.password);
			pass = true;
		} else if (key == "version" && register_config) {
			index = get_value(json, index, registerConfigReq.version);
			version = true;
		} else {
			printf("Invalid json, key: %s\njson object\n%s\n", key.c_str(), json.c_str());
			throw json_parser_exception{invalid_json};
		}
	
//		printf("index: %d, value: %s\n", index, value.c_str());
		
	} while (!email || !pass || !version);
	
//	if (json[index] != '}') {
//		printf("Invalid json, expected '}': %s\n", json.c_str());
//		throw json_parser_exception{"Invalid json Exception"};
//	}
	verify_close_of_object(json, index);
	
	return registerConfigReq;
}


sync_initial_request json_parser::parse_sync_initial(std::string& json)
{
	int index = get_start_of_object(json, 0);
	
	sync_initial_request syncReq;
	std::string key{};
	std::string value{};
	bool accounts{false}, pass{false}, user{false};
	
	do {
		index = get_key(json, index, key);
		
		if (key == "accounts") {
			if (json[index] != ':') {
				printf("invalid json, expected  ':' at index %d, char: %c\n%s\n", index, json[index], json.c_str());
				throw json_parser_exception{invalid_json};
			}
			
			index = eat_white_sapce(json, ++index);
			
			if (json[index] != '[') {
				printf("invalid json, expected  '[' at index %d, char: %c\n%s\n", index, json[index], json.c_str());
				throw json_parser_exception{invalid_json};
			}
			
			index = eat_white_sapce(json, ++index);

			if (json[index] != ']') {
				index = get_start_of_object(json, index);
				
				while (true) {
					bool name{false}, time{false};
					std::string accountName;
					long long updateTime;
					
					do {
						index = get_key(json, index, key);

						if (key == "accountName") {
							index = get_value(json, index, accountName);
							name = true;
						} else if (key == "updateTime") {
							index = get_long_long_value(json, index, updateTime);
							time = true;
						} else {
							printf("invalid json, invalid key at index %d, char: %c\n%s\n", index, json[index], json.c_str());
							throw json_parser_exception{invalid_json};
						}
					} while (!name || !time);

					index = verify_close_of_object(json, index);
					syncReq.accounts.emplace(syncReq.accounts.begin(), accountName, updateTime);
					
					if (json[index] == ',') {
						index = get_start_of_object(json, ++index);
					} else if (json[index] == ']') {
						index = eat_white_sapce(json, ++index);
						accounts = true;
						
						if (json[index] == ',')
							index = eat_white_sapce(json, ++index);
							
						break;
					} else {
						printf("invalid json, expected ',' of ']' at index %d, char: %c\n%s\n", index, json[index], json.c_str());
						throw json_parser_exception{invalid_json};
					}
				}
			} else {
				index = eat_white_sapce(json, ++index);
				
				if (json[index] == ',') {
					index = eat_white_sapce(json, ++ index);
				}
			}
			
		} else if (key == "user") {
			index = get_value(json, index, syncReq.registerConfigReq.email);
			user = true;
		} else if (key == "password") {
			index = get_value(json, index, syncReq.registerConfigReq.password);
			pass = true;
		}
		
	} while (!accounts || !pass || !user);
	
	verify_close_of_object(json, index);
	
	return syncReq;
}


sync_final_request json_parser::parse_sync_final(std::string& json)
{
	int index = get_start_of_object(json, 0);
	
	sync_final_request syncFinal;
	std::string key{};
	std::string value{};
	bool accounts{false}, pass{false}, user{false}, lockTime{false};
	
	do {
		index = get_key(json, index, key);
		
		if (key == "user") {
			index = get_value(json, index, syncFinal.user);
			user = true;
		} else if (key == "password") {
			index = get_value(json, index, syncFinal.password);
			pass = true;
		} else if (key == "lockTime") {
			index = get_long_long_value(json, index, syncFinal.lockTime);
			lockTime = true;
		} else if (key == "accounts") {
			
			if (json[index] != ':') {
				printf("invalid json, expected  ':' at index %d, char: %c\n%s\n", index, json[index], json.c_str());
				throw json_parser_exception{invalid_json};
			}
			
			index = eat_white_sapce(json, ++index);
			
			if (json[index] != '[') {
				printf("invalid json, expected  '[' at index %d, char: %c\n%s\n", index, json[index], json.c_str());
				throw json_parser_exception{invalid_json};
			}
			
			index = eat_white_sapce(json, ++index);

			if (json[index] != ']') {
				index = get_start_of_object(json, index);
				
				while (true) {
					bool name{false}, time{false}, deleted{false}, user{false}, url{false},
							pass{false}, oldPass{false};
					
					Account account;
					
					do {
						index = get_key(json, index, key);

						if (key == "accountName") {
							index = get_value(json, index, account.account_name);
							name = true;
						} else if (key == "updateTime") {
							index = get_long_long_value(json, index, account.update_time);
							time = true;
						} else if (key == "password") {
							index = get_value(json, index, account.password);
							pass = true;
						} else if (key == "oldPassword") {
							index = get_value(json, index, account.old_password);
							oldPass = true;
						} else if (key == "url") {
							index = get_value(json, index, account.url);
							url = true;
						} else if (key == "userName") {
							index = get_value(json, index, account.user_name);
							user = true;
						} else if (key == "deleted") {
							index = get_bool_value(json, index, account.deleted);
							deleted = true;
						} else {
							printf("invalid json, invalid key at index %d, char: %c\n%s\n", index, json[index], json.c_str());
							throw json_parser_exception{invalid_json};
						}
					} while (!name || !time || !deleted || !user || !url || !pass || !oldPass);
					
					index = verify_close_of_object(json, index);
					syncFinal.accounts.push_back(account);
					
					if (json[index] == ',') {
						index = get_start_of_object(json, ++index);
					} else if (json[index] == ']') {
						index = eat_white_sapce(json, ++index);
						accounts = true;
						
						if (json[index] == ',')
							index = eat_white_sapce(json, ++index);
							
						break;
					} else {
						printf("invalid json, expected ',' of ']' at index %d, char: %c\n%s\n", index, json[index], json.c_str());
						throw json_parser_exception{invalid_json};
					}
				}
			} else {
				index = eat_white_sapce(json, ++index);
				
				if (json[index] == ',') {
					index = eat_white_sapce(json, ++ index);
				}
				
				accounts = true;
			}
		}
		
	} while (!accounts || ! pass || !user || !lockTime);

	verify_close_of_object(json, index);
	
	return syncFinal;
}


int json_parser::eat_white_sapce(std::string& json, int index)
{
	while (json[index] == ' ' || json[index] == '\t' || json[index] == '\n' || json[index] == '\r')
		index++;
		
	return index;
}

// revist to see if need to worry about keys with a " in them?
int json_parser::get_key(std::string& json, int index, std::string& key)
{
	if (json[index] != '"') {
		printf("invalid json, in get_key at index %d, char: %c\n%s\n", index, json[index], json.c_str());
		printf("int value: %d\n", ((int)json[index]));
		throw json_parser_exception{invalid_json};
	}
	
	int start = ++index;
	
	while (json[index] != '"')
		index++;
		
	key = json.substr(start, index-start);
	
	return eat_white_sapce(json, ++index);
}


int json_parser::get_value(std::string& json, int index, std::string& value)
{
	if (json[index] != ':') {
		printf("invalid json, in get_value at index %d, char: %c\n%s\n", index, json[index], json.c_str());
		throw json_parser_exception{invalid_json};
	}
	
	index = eat_white_sapce(json, ++index);
	index = get_key(json, index, value);
	index = eat_white_sapce(json, index);
	
	if (json[index] == ',') {
		index = eat_white_sapce(json, ++index);
	}
		
	return index;
}


int json_parser::get_long_long_value(std::string& json, int index, long long& value)
{
	if (json[index] != ':') {
		printf("invalid json, in get_value at index %d, char: %c, expect ':'\n%s\n", index, json[index], json.c_str());
		throw json_parser_exception{invalid_json};
	}
	
	index = eat_white_sapce(json, ++index);
	int start = index;

	while (json[index] >= 48 && json[index] <= 57)
		index++;
	
	const std::string tmp = json.substr(start, index-start);

	//TODO TODO error checking
	value = std::atoll(tmp.c_str());
	
	return eat_white_sapce(json, (json[index] == '}' || json[index] == ']') ? index : ++index);
}


int json_parser::get_bool_value(std::string& json, int index, bool& value)
{
	if (json[index] != ':') {
		printf("invalid json, in get_value at index %d, char: %c, expect ':'\n%s\n", index, json[index], json.c_str());
		throw json_parser_exception{invalid_json};
	}
	
	index = eat_white_sapce(json, ++index);
	int start = index;

//	while (json[index] != ' ' && json[index] != '\t' && json[index] != '\n' && json[index] != ',' &&
//		   json[index] != '}' && json[index] != ']')
//		index++;
		
	while ((json[index] >= 65 && json[index] <= 90) || (json[index] >= 97 && json[index] <= 122))
		index++;
	
	const std::string tmp = json.substr(start, index-start);

	if (tmp == "true" || tmp == "True") {
		value = true;
	} else if (tmp == "false" || tmp == "False") {
		value = false;
	} else {
		printf("invalid json, expected 'bool' value at %d, found: %s\n%s\n", index, tmp.c_str(), json.c_str());
		throw json_parser_exception{invalid_json};
	}
	
	return eat_white_sapce(json, (json[index] == '}' || json[index] == ']') ? index : ++index);
}


int json_parser::verify_close_of_object(std::string& json, int index)
{
	if (json[index] != '}') {
		printf("Invalid json, expected '}' at index %d, :\n %s\n", index, json.c_str());
		throw json_parser_exception{invalid_json};
	}
	
	return eat_white_sapce(json, ++index);
}


int json_parser::get_start_of_object(std::string& json, int index)
{
	index = eat_white_sapce(json, index);
	
	if (json[index] != '{') {
		printf("Invalid json, index %d has char %c\n%s\n", index, json[index], json.c_str());
		throw json_parser_exception{invalid_json};
	} 
	
	return eat_white_sapce(json, ++index);
}
