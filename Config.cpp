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
	} else if (strcmp(key, "max_service_threads") == 0) {
		max_service_threads = atoi(value);
	} else if (strcmp(key, "ssl") == 0) {
		ssl = (strcmp(value, "true") == 0) ? true: false;
	} else if (strcmp(key, "server_cert") == 0) {
		server_cert = value;
	} else if (strcmp(key, "server_key") == 0) {
		server_key = value;
	} else if (strcmp(key, "db_server") == 0) {
		db_server = value;
		storeType = postgres_store;
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
	} else if (strcmp(key, "db_cleaner_history_purge_days") == 0) {
		db_cleaner_history_purge_days = atoi(value);
	} else if (strcmp(key, "server_max_connections") == 0) {
		server_max_connections = atoi(value);
	} else if (strcmp(key, "server_keep_alive") == 0) {
		server_keep_alive = atoi(value);
	} else if (strcmp(key, "server_use_keep_alive_cleaner") == 0) {
		server_use_keep_alive_cleaner = (strcmp(value, "true") == 0) ? true: false;
	} else if (strcmp(key, "server_tcp_back_log") == 0) {
		server_tcp_back_log = atoi(value);
	} else if (strcmp(key, "max_account_pending_ops") == 0) {
		max_account_pending_ops = atoi(value);
	} else if (strcmp(key, "db_max_connections") == 0) {
		db_max_connections = atoi(value);
	} else if (strcmp(key, "db_min_connections") == 0) {
		db_min_connections = atoi(value);
	} else if (strcmp(key, "redis_server") == 0) {
		redis_server = value;
		storeType = redis_store;
	} else if (strcmp(key, "redis_port") == 0) {
		redis_port = atoi(value);
	} else if (strcmp(key, "redis_username") == 0) {
		redis_username = value;
	} else if (strcmp(key, "redis_password") == 0) {
		redis_password = value;
	} else if (strcmp(key, "redis_min_connections") == 0) {
		redis_min_connections = atoi(value);
	} else if (strcmp(key, "redis_max_connections") == 0) {
		redis_max_connections = atoi(value);
	} else if (strcmp(key, "redis_cleaner") == 0) {
		redis_cleaner = (strcmp(value, "true") == 0) ? true: false;
	} else if (strcmp(key, "redis_cleaner_interval") == 0) {
		redis_cleaner_interval = atoi(value);
	} else if (strcmp(key, "redis_cleaner_purge_days") == 0) {
		redis_cleaner_purge_days = atoi(value);
	} else if (strcmp(key, "redis_cleaner_history_purge_days") == 0) {
		redis_cleaner_history_purge_days = atoi(value);
	}
	
}


void Config::debugValues()
{
	std::cout << "Config Values:" << std::endl;
	std::cout << "bind address: " << bind_address << std::endl;
	std::cout << "bind port: " << bind_port << std::endl;
	std::cout << "service threads: " << service_threads << std::endl;
	std::cout << "max service threads: " << max_service_threads << std::endl;
	std::cout << "ssl: " << ssl << std::endl;
	std::cout << "server cert: " << server_cert << std::endl;
	std::cout << "server key" << server_key << std::endl;
	std::cout << "server max connections: " << server_max_connections << std::endl;
	std::cout << "server use keep alive: " << server_use_keep_alive_cleaner << std::endl;
	std::cout << "server keep alive: " << server_keep_alive << std::endl;
	std::cout << "server tcp backlog: " << server_tcp_back_log << std::endl;
	std::cout << "Account max pending operations count: " << max_account_pending_ops << std::endl;
	std::cout << "db server: " << db_server << std::endl;
	std::cout << "db port: " << db_port << std::endl;
	std::cout << "db user: " << db_username << std::endl;
	std::cout << "db pass: " << db_password << std::endl;
	std::cout << "db name: " << db_name << std::endl;
	std::cout << "db min connections: " << db_min_connections << std::endl;
	std::cout << "db max connections: " << db_max_connections << std::endl;
	std::cout << "db cleaner: " << db_cleaner << std::endl;
	std::cout << "db cleaner interval: " << db_cleaner_interval << std::endl;
	std::cout << "db cleaner purge days: " << db_cleaner_purge_days << std::endl;
	std::cout << "db cleaner history purge days: " << db_cleaner_history_purge_days << std::endl;
	
	std::cout << "redis_server: " << redis_server << std::endl;
	std::cout << "redis_port: " << redis_port << std::endl;
	std::cout << "redis_username: " << redis_username << std::endl;
	std::cout << "redis_password: " << redis_password << std::endl;
	std::cout << "redis_min_connections: " << redis_min_connections << std::endl;
	std::cout << "redis_max_connections: " << redis_max_connections << std::endl;
	std::cout << "redis cleaner: " << redis_cleaner << std::endl;
	std::cout << "redis cleaner interval: " << redis_cleaner_interval << std::endl;
	std::cout << "redis cleaner purge days: " << redis_cleaner_purge_days << std::endl;
	std::cout << "redis cleaner history purge days: " << redis_cleaner_history_purge_days << std::endl;
	
	if (storeType == postgres_store) {
		std::cout << "\nUsing Store Type: Postgres\n" << std::endl;
	} else if (storeType == redis_store) {
		std::cout << "\nUsing Store Type: Redis\n" << std::endl;
	} else {
		std::cout << "\nERROR - No store type configured\n" << std::endl;
	}
}
