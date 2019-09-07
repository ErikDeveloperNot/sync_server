#ifndef _SERVER_H_
#define _SERVER_H_

#include <string>
#include <queue>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

#include "openssl/ssl.h"
#include "openssl/err.h"
#include "config_http.h"
#include "Config.h"

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
	
	// service thread variables
	std::vector<std::thread> service_threads;
	std::queue<conn_meta *> service_q;
	std::mutex service_mutex;
	std::condition_variable cv;
	std::atomic_int current_connections;
	
	SSL_CTX *ctx;

	std::map<std::string, User_info> infos;
	std::map<int, conn_meta> conn_map;
	
	// socket server variables
	fd_set connection_fds;
	fd_set read_fds;
	std::mutex connection_fds_mutex;
	int rv;
	int served;
	int max_connections;
	int listen_sd;
	int max_fd;
	
	// variables for file descriptor used by service threads
	int control[2];
	char control_buf[10];
	FILE *control_file;
	
//	int control_keep_alive[2];
//	char control_keep_alive_buf[10];
//	FILE *control_keep_alive_file;
	
	
	int startListener(Config *config);
	void handleServerBusy(int);
	void close_client(int, SSL *, fd_set &, std::mutex &);
	long long current_time_ms();

public:
	server(Config *config);
	~server() = default;

	void start();
	void shutdown(bool immdeiate);
};

#endif // _SERVER_H_
