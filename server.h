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
	long last_used;
	bool active;
	
	conn_meta(int s, SSL *_ssl, long l, bool a) : socket{s}, ssl{_ssl}, last_used{l}, active{a} {}
	conn_meta() = default;
};



class server
{
private:
	
	std::string db_connection_string;
	Config *config;
	config_http configHttp;
	
	// service thread variables
//	std::vector<std::thread> service_threads;
//	std::vector<int> service_threads_running;
	int current_thread_count;
//	long last_thread_check;
//	int thread_check_counter;
	std::queue<conn_meta *> service_q;
	std::mutex service_mutex;
	std::condition_variable cv;
	std::atomic_int current_connections;
	conn_meta shut_thread_down_meta {-1, nullptr, END_IT, false};
	
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
	
	
	int startListener(Config *config);
	void handleServerBusy(int);
	void close_client(int, SSL *, fd_set &, std::mutex &);
	long long current_time_ms();
	long current_time_sec();
	void start_service_thread();
	void start_service_thread_manager();
	void close_old_connections();
	void start_client_seesion_manager();
//	void check_threads();

public:
	server(Config *config);
	~server() = default;

	void start();
	void shutdown(bool immdeiate);
};


void service_thread(std::queue<conn_meta *> &q, std::mutex &q_mutex, std::condition_variable &cv, 
					 SSL_CTX *ctx, std::atomic_int &connections, Config *config, 
					 std::map<std::string, User_info> &, fd_set &, std::map<int, conn_meta> &,
					 std::mutex &fd_set_mutex, int);



#endif // _SERVER_H_
