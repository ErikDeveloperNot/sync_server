#ifndef _SERVER_H_
#define _SERVER_H_

#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>

#include "openssl/ssl.h"
#include "openssl/err.h"

#define END_IT -99

class Config;


class server
{
private:
	
	std::string db_connection_string;
	Config *config;
	
	std::queue<int> service_q;
	std::mutex service_mutex;
	std::condition_variable cv;
	std::atomic_int current_connections;
	
	SSL_CTX *ctx;


	int startListener(Config *config);
	void handleServerBusy(int);
public:
	server(Config *config);
	~server();

};

#endif // _SERVER_H_
