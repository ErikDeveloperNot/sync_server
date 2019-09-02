#ifndef _SERVER_H_
#define _SERVER_H_

#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>

#include "openssl/ssl.h"
#include "openssl/err.h"
#include "config_http.h"

#define END_IT -99
#define STALE_TIMEOUT 30

class Config;


struct conn_meta
{
	int socket;
	SSL *ssl;
	long long last_used;
	bool active;
	
	conn_meta(int s, SSL *_ssl, long long l, bool a) : socket{s}, ssl{_ssl}, last_used{l}, active{a} {}
	conn_meta() = default;
};


class server
{
private:
	
	std::string db_connection_string;
	Config *config;
	config_http configHttp;
	
//	std::queue<int> service_q;
	std::queue<conn_meta *> service_q;
	std::mutex service_mutex;
	std::condition_variable cv;
	std::atomic_int current_connections;
	
	SSL_CTX *ctx;


	int startListener(Config *config);
	void handleServerBusy(int);
	void close_client(int, SSL *, fd_set &, std::mutex &);
	long long current_time_ms();
public:
	server(Config *config);
	~server();

};

#endif // _SERVER_H_
