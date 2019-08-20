#include "Config.h"

#include <iostream>
#include <cstring>

#include <stdio.h>


Config::Config(char *_config)
{
	FILE *file = fopen(_config, "r");
	
	if (file != NULL) {
		char line[1024];

		while (fgets(line, 1023, file) != NULL) {
			char *found = strstr(line, "=");
			
			if (found) {
				char *token = strtok(line, "=");

				if (token) {
					char *key = token;
					token = strtok(NULL, "\n");
					setKeyValue(key, token);
				}
				
			}
		}
	
		fclose(file);
	} else {
		std::cout << "Config file: " << _config << ", failed to open, using defaults" << std::endl;
	}
	
	debugValues();
}

Config::~Config()
{
}


void Config::setKeyValue(char* key, char* value)
{
	if (strcmp(key, "bind_address") == 0) {
		bind_address = value;
	} else if (strcmp(key, "bind_port") == 0) {
		bind_port = atoi(value);
	} else if (strcmp(key, "service_threads") == 0) {
		service_threads = atoi(value);
	} else if (strcmp(key, "ssl") == 0) {
		ssl = (strcmp(value, "true") == 0) ? true: false;
	} else if (strcmp(key, "server_cert") == 0) {
		server_cert = value;
	} else if (strcmp(key, "server_key") == 0) {
		server_key = value;
	} else if (strcmp(key, "db_server") == 0) {
		db_server = value;
	} else if (strcmp(key, "db_port") == 0) {
		db_port = value;
	} else if (strcmp(key, "db_password") == 0) {
		db_password = value;
	} else if (strcmp(key, "db_username") == 0) {
		db_username = value;
	} else if (strcmp(key, "db_name") == 0) {
		db_name = value;
	} else if (strcmp(key, "db_cleaner") == 0) {
		db_cleaner = (strcmp(value, "true") == 0) ? true: false;
	} else if (strcmp(key, "db_cleaner_interval") == 0) {
		db_cleaner_interval = atoi(value);
	} else if (strcmp(key, "db_cleaner_purge_days") == 0) {
		db_cleaner_purge_days = atoi(value);
	} else if (strcmp(key, "server_max_connections") == 0) {
		server_max_connections = atoi(value);
	}
}


void Config::debugValues()
{
	std::cout << "Config Values:" << std::endl;
	std::cout << "bind address: " << bind_address << std::endl;
	std::cout << "bind port: " << bind_port << std::endl;
	std::cout << "service threads: " << service_threads << std::endl;
	std::cout << "ssl: " << ssl << std::endl;
	std::cout << "server cert: " << server_cert << std::endl;
	std::cout << "server key" << server_key << std::endl;
	std::cout << "server max connections: " << server_max_connections << std::endl;
	std::cout << "db server: " << db_server << std::endl;
	std::cout << "db port: " << db_port << std::endl;
	std::cout << "db user: " << db_username << std::endl;
	std::cout << "db pass: " << db_password << std::endl;
	std::cout << "db name: " << db_name << std::endl;
	std::cout << "db cleaner: " << db_cleaner << std::endl;
	std::cout << "db cleaner interval: " << db_cleaner_interval << std::endl;
	std::cout << "db cleaner purge days: " << db_cleaner_purge_days << std::endl;
}