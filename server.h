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
//#include "data_store_connection.h"
#include "IDataStore.h"


// command code to shutdown
#define END_IT 					-99
// time out for cleaner thread for stale connections
#define STALE_TIMEOUT 			30
// number of SSL socket read attempts for throwing
#define SSL_READ_ATTEMPTS		10

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
	int current_thread_count;
	std::atomic_int active_threads;
	std::queue<conn_meta *> service_q;
	std::mutex service_mutex;
	std::condition_variable cv;
	std::atomic_int current_connections;
	conn_meta shut_thread_down_meta {-1, nullptr, END_IT, false};
	
	SSL_CTX *ctx;

	std::map<char *, User_info, cmp_key> infos;
	std::map<int, conn_meta> conn_map;
	
//	data_store_connection store;
	IDataStore *store;
	
	// socket server variables
	fd_set connection_fds;
	fd_set read_fds;
	std::mutex connection_fds_mutex;
	std::atomic_bool read_fds_called;
	int rv;
	int served;
	int max_connections;
	int listen_sd;
	int max_fd;
	
	// set socket read/write timeout
	timeval rw_to;
	
	// variables for file descriptor used by service threads
	int control[2];
	char control_buf[10];
	FILE *control_file;
	
	// variables for file descriptor used by service threads
	int control2[2];
	char control2_buf[10];
	FILE *control2_file;
	std::vector<int> clients_to_close;
	std::mutex clients_to_close_mux;
	
	
	int startListener(Config *config);
	void handleServerBusy(int);
	void close_client(int, SSL *, fd_set &, std::mutex &);
	void close_clients(std::vector<int> clients);
	long long current_time_ms();
	long current_time_sec();
	void start_service_thread();
	void start_service_thread_manager();
	void start_client_seesion_manager();

public:
	server(Config *config);
	~server() = default;

	void start();
	void shutdown(bool immdeiate);
};


//void service_thread(std::queue<conn_meta *> &q, std::mutex &q_mutex, std::condition_variable &cv, 
//					 SSL_CTX *ctx, std::atomic_int &connections, Config *config, 
//					 std::map<char *, User_info, cmp_key> &, fd_set &, std::mutex &fd_set_mutex, int, 
//					 std::atomic_int &active_threads, data_store_connection &store, std::atomic_bool &read_fds_called); /*, std::vector<int> &, std::mutex &,
//					 int);*/
void service_thread(std::queue<conn_meta *> &q, std::mutex &q_mutex, std::condition_variable &cv, 
					 SSL_CTX *ctx, std::atomic_int &connections, Config *config, 
					 std::map<char *, User_info, cmp_key> &, fd_set &, std::mutex &fd_set_mutex, int, 
					 std::atomic_int &active_threads, IDataStore *store, std::atomic_bool &read_fds_called); /*, std::vector<int> &, std::mutex &,
					 int);*/



#endif // _SERVER_H_
