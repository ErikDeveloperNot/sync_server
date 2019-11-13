#include "server.h"
#include "sync_handler.h"
#include "config_http.h"
#include "register_server_exception.h"

#include <iostream>
#include <atomic>
#include <string>
#include <sstream>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <resolv.h>

#include <errno.h>
#include <unistd.h>

/*
 * Main thread will handle all incoming network requests
 * each request will be past off to a service thread
 * which will handle all requests/response
 * 
 * v1: 
 * simple, close each connection after write
 * a single lock for entire user map
 * 
 * v1.5:
 * only request where connection reuse should be needed is between sync-initial/final
 * so modify thread that handles initial to keep connection open and block on read with short timeout timeout
 * 
 * v2:
 * will default to keep-alive
 * Issue with current design of handing sockets off to worker thread and having it do the 
 * read. While the client is working the main thread running select will get notifcations again
 * for the fd it gave the worker thread. 3 possible solutions
 * 1. remove the fd from the master FD set, when thread is done with the fd request add it back to
 *    the master set. this requires a way to signal main to break out of select and read the master
 *    again. Possible introduce a single control fd to the set that all clients can modify each time
 *    they are done with an fd to wake of main from the select.
 * 2. have the main thread read a request before handing it off. I would think I would want to avoid
 *    this as a slow read will cause a backup, etc
 * 3. the one I will go with first is introduce a flag for each conn_meta to signal if a thread is working
 *    that request, if so main will ignore its wake up for that fd. This might cause to many select wakeups
 *    but I assume the call is fairly low overhead.
 *    
 * 
 */




//void service_thread(std::queue<conn_meta *> &q, std::mutex &q_mutex, std::condition_variable &cv, 
//					 SSL_CTX *ctx, std::atomic_int &connections, Config *config, 
//					 std::map<std::string, User_info> &, fd_set &, std::map<int, conn_meta> &,
//					 std::mutex &fd_set_mutex, int);
					 


server::server(Config *conf)
{
	config = conf;

	FD_ZERO(&connection_fds);
	FD_ZERO(&read_fds);
	max_connections = config->getServerMaxConnections();
	
	pipe(control);
	FD_SET(control[0], &connection_fds);
	control_file = fdopen( control[0], "r" );
	
	pipe(control2);
	FD_SET(control2[0], &connection_fds);
	control2_file = fdopen(control2[0], "r" );
	
	listen_sd = startListener(config);
	max_fd = listen_sd;
	FD_SET(listen_sd, &connection_fds);
	read_fds_called = false;
	
	printf("\n\n");
	printf("Server fd: %d, max connections: %d\n", listen_sd, max_connections);
	printf("control read fd: %d\n", control[0]);
	printf("control write fd: %d\n", control[1]);
	printf("control2 read fd: %d\n", control2[0]);
	printf("control2 write fd: %d\n", control2[1]);
	
	current_thread_count = 0;
	active_threads = 0;
	
	store.initialize(config);
	
	for (int i{1}; i<=config->getServiceThreads(); i++) {
		start_service_thread();
	}
	
	start_service_thread_manager();
	
	if (config->isServerUseKeepAliveCleaner())
		start_client_seesion_manager();
		
}


void server::start()
{
	int max_service_threads = config->getMaxServiceThreads();
		
	while (true) {
		connection_fds_mutex.lock();
		read_fds = connection_fds;
		connection_fds_mutex.unlock();
		served = 0;

		rv = select(max_fd+1, &read_fds, NULL, NULL, NULL);
		
		int current_q_sz = service_q.size();
		printf("SELECT returned: %d\n", rv);
		printf("queue size: %d, current threads: %d, active threads: %d\n", current_q_sz, current_thread_count, active_threads.load());

		if (current_thread_count < max_service_threads && current_q_sz > current_thread_count && 
			active_threads > 0 && current_thread_count/active_threads < 2) {
			int cnt = (current_thread_count/2 < max_service_threads) ? current_thread_count/2 : max_service_threads;
			
			for (size_t i{0}; i < cnt; i++) 
				start_service_thread();
		}


		if (rv < 0) {
			//error
			printf("Error received doing select(), try to find offending fd\n");
			perror("select");
		} else {
			printf("max_fd = %d\n", max_fd);
			if (FD_ISSET(control[0], &read_fds)) {
				printf(">>>>>>>>>>>>> Re-read fds CONTROL %d was modified\n", control[0]);
connection_fds_mutex.lock();
				fgets(control_buf, 2, control_file);
read_fds_called = false;
connection_fds_mutex.unlock();
		
				if (rv == 1)
					continue;
				else
					served++;
			}
		
			if (FD_ISSET(control2[0], &read_fds)) {
				printf(">>>>>>>>>>>>> Close clients CONTROL %d was modified\n", control2[0]);
//clients_to_close_mux.lock();
				fgets(control2_buf, 2, control2_file);
				served++;
				
				clients_to_close_mux.lock();
				close_clients(clients_to_close);
				clients_to_close.clear();
				clients_to_close_mux.unlock();
			}
			
			if (FD_ISSET(listen_sd, &read_fds)) {
				//handle new connection
				struct sockaddr_in addr;
				socklen_t len = sizeof(addr);
						
				int client = accept(listen_sd, (struct sockaddr*)&addr, &len);
				printf("Connection: %s:%d, client fd:%d\n",inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), client);
						
				if (current_connections >= max_connections) {
					printf("Max Connections %d reached, rejecting\n", static_cast<int>(current_connections));
					handleServerBusy(client);
					continue;
				}

				if (client > max_fd)
					max_fd = client;
					
				conn_map[client] = conn_meta(client, NULL, current_time_sec(), true);   //revisit time if I use this field
				current_connections++;
				service_mutex.lock();
				service_q.push(&(conn_map[client]));
				service_mutex.unlock();
				cv.notify_one();
			
				served++;
			}

//			long long now = current_time_ms();
			
			for (int i=0; i<=max_fd; i++) {
				if (rv == served) {
					//all active fd handled, break
					break;
				}
				
				if (i == control[0] || i == listen_sd || i == control2[0])
					continue;
					
				if (FD_ISSET(i, &read_fds)) {
					served++;
					
					if (conn_map[i].active)		//fd is already being served
							continue;
							
					int n = 0;
					ioctl(i, FIONREAD, &n);

					if (n == 0) {
						close_client(i, conn_map[i].ssl, connection_fds, connection_fds_mutex);
						current_connections--;
						conn_map.erase(i);
					} else if (n > 0) {
						//TODO - revist to verify this client is not already active, if so ignore this
						printf(">>Removing client %d from connections_fds\n", i);
						
						connection_fds_mutex.lock();
						FD_CLR(i, &connection_fds);
						connection_fds_mutex.unlock();
						
						std::lock_guard<std::mutex> lg{service_mutex};
						conn_map[i].active = true;
						service_q.push(&(conn_map[i]));
						cv.notify_one();
					} else {
						printf("ERRRRRRROOOOORRRR - dont think i should see\n");
					}
				} 
			}
		}
	}
}



int server::startListener(Config *config)
{
	//believe this only needs to be called once
	SSL_library_init();
	/* load & register all cryptos, etc. */
	OpenSSL_add_all_algorithms();
	/* load all error messages */  
    SSL_load_error_strings();   
	
//	SSL_METHOD *method;
	const SSL_METHOD *method = TLS_server_method();
	
	/* create new context from method */
	ctx = SSL_CTX_new(method);   
	
//set session timeout for persistent connections
//SSL_CTX_set_timeout(ctx, 15L);		//only needed for sync, if client takes more then 5 seconds slow client/net
    
	if ( ctx == NULL )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
	
	/* set the local certificate from CertFile */
    if ( SSL_CTX_use_certificate_file(ctx, config->getServerCert().c_str(), SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    /* set the private key from KeyFile (may be the same as CertFile) */
    if ( SSL_CTX_use_PrivateKey_file(ctx, config->getServerKey().c_str(), SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    /* verify private key */
    if ( !SSL_CTX_check_private_key(ctx) )
    {
        fprintf(stderr, "Private key does not match the public certificate\n");
        abort();
    }
	
	
	int sd;
    struct sockaddr_in addr;
 
    sd = socket(PF_INET, SOCK_STREAM, 0);
	int yes{1};
	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config->getBindPort());

//revist to loop through lookups like in select_server example	
//	if (strcmp("any", config->getBindAddress().c_str()) == 0)
		addr.sin_addr.s_addr = INADDR_ANY;
//	else
//		addr.sin_addr.s_addr = config->getBindAddress().c_str();
    
	if (bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 )
    {
		perror("can't bind port");
		abort();
    }
    
	if ( listen(sd, config->getServerTcpBackLog()) != 0 )
    {
		perror("Can't configure listening port");
		abort();
    }
    
	return sd;
}


void server::handleServerBusy(int client)
{
	SSL *ssl = SSL_new(ctx);
	SSL_set_fd(ssl, client); 
		
	if ( SSL_accept(ssl) <= 0 )  {   /* do SSL-protocol accept/handshake */
		ERR_print_errors_fp(stderr);
	} else {
		const char * reply = configHttp.build_reply(HTTP_503, close_con);
		SSL_write(ssl, reply, strlen(reply));
	}
	
	SSL_free(ssl);         /* release SSL state */
	close(client);

}


void server::close_client(int fd, SSL *ssl, fd_set & connections_fd, std::mutex &connection_fds_mutex)
{
	printf("closing client: %d\n", fd);
	connection_fds_mutex.lock();
	FD_CLR(fd, &connections_fd);
	connection_fds_mutex.unlock();

	SSL_free(ssl);          /* release SSL state */
	close(fd);
	printf("client %d should be closed\n", fd);
}


void server::close_clients(std::vector<int> clients)
{
	connection_fds_mutex.lock();
	
	for (auto &val : clients) 
		FD_CLR(val, &connection_fds);
		
	connection_fds_mutex.unlock();
	
	for (auto &val : clients) {
//		if (conn_map.count(val) > 0) {
if (conn_map.count(val) > 0 && !conn_map[val].active) {
printf("SSL_free: \n");
//if (!conn_map[val].ssl)		
			SSL_free(conn_map[val].ssl);
			close(val);

			current_connections--;
			conn_map.erase(val);
			printf("client %d should be closed\n", val);
		}
	}
}


void server::start_client_seesion_manager()
{
	printf("Starting client session manager thread\n");
	int keep_alive = config->getServerKeepAlive();
	
	std::thread manager([&]() {
		while(true) {
			long current_time = current_time_sec();
			
			if (clients_to_close.size() < 1) {
				clients_to_close_mux.lock();
				
				for (int i{0}; i <= max_fd; i++) {
					if (conn_map.count(i) > 0 && !conn_map[i].active && conn_map[i].socket > 0 && 
												current_time - conn_map[i].last_used >= keep_alive) 
						clients_to_close.push_back(i);
				}
				
				clients_to_close_mux.unlock();
				
				if (clients_to_close.size() > 0) {
clients_to_close_mux.lock();
					write(control2[1], control2_buf, 1);
clients_to_close_mux.unlock();
				}
			}
			
			std::this_thread::sleep_for(std::chrono::seconds(keep_alive));
		}
	});
	
	manager.detach();
}


long long server::current_time_ms()
{
	return std::chrono::system_clock::now().time_since_epoch().count()/1000;
}


long server::current_time_sec()
{
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}


void server::shutdown(bool immdeiate)
{
	if (!immdeiate) {
		printf("Server shutting down, waiting for threads to complete\n");
			
		for (size_t i{0}; i < current_thread_count; i++) {
			std::lock_guard<std::mutex> lg{service_mutex};
			service_q.push(&shut_thread_down_meta); 
			cv.notify_one();
		}
	
		while (true) {
			service_mutex.lock();
			
			if (service_q.size() > 0) {
				service_mutex.unlock();
				cv.notify_one();
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				break;
			}
		}

	}
	
	
	printf("Server done\n");
}


void server::start_service_thread()
{
	current_thread_count++;
	
	std::thread t{service_thread, std::ref(service_q), std::ref(service_mutex), 
						std::ref(cv), ctx, std::ref(current_connections), config, std::ref(infos),
						std::ref(connection_fds), std::ref(connection_fds_mutex), control[1], std::ref(active_threads), 
						std::ref(store), std::ref(read_fds_called)}; //, std::ref(clients_to_close), std::ref(clients_to_close_mux), control2[1]};
	
		std::stringstream ss;
		ss << t.get_id();
		std::string t_id{"service_thread_"};
		t_id += ss.str();
		printf("Thread: %s, started, current thread count: %d\n", t_id.c_str(), current_thread_count);
		
		t.detach();
}


void server::start_service_thread_manager()
{
	std::thread manager([&]() {
		int server_threads = config->getServiceThreads();
		int thread_check_counter{0};
		
		while (true) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10000));
//			long now = current_time_sec();
			int current_q_sz = service_q.size();
			
			if (current_q_sz < current_thread_count/0.2) {
				if (++thread_check_counter > 20) {
					int to_shutdown = (current_thread_count - server_threads) / 2;
					
					if (to_shutdown > 0) {
						for (int i{0}; i<to_shutdown; i++) {
							printf("Shutting down a thread, current thread count: %d\n", --current_thread_count);
							service_mutex.lock();
							service_q.push(&shut_thread_down_meta); 
							service_mutex.unlock();
							cv.notify_one();
						}
					}
					
					thread_check_counter = 0;
				}
			} else {
				thread_check_counter = (--thread_check_counter < 0) ? 0 : thread_check_counter;
			}
		}
	});
		
	manager.detach();
}


/*
 *  
 * Service Threads Area Below
 * 
 */

// functions used by service thread
bool verify_request(std::string &method, std::string &operation);
bool read_incoming_bytes(SSL *, std::string &msg, int contentLenth);
bool parse_header(std::string &header, std::string &operation, std::string &contentLenth, request_type &requestType);
//void close_client(conn_meta *, std::mutex &, std::atomic_int &, std::vector<int> &, int, char *);
void close_client(conn_meta *, std::atomic_int &);

// definition of service thread-
//void service_thread(std::queue<conn_meta *> &q, std::mutex &q_mutex, std::condition_variable &cv, 
//					 SSL_CTX *ctx, std::atomic_int &connections, Config *config, 
//					 std::map<std::string, User_info> & infos, fd_set & connections_fd, 
//					 std::mutex &connections_fd_mutex, int control, std::atomic_int & active_threads, 
//					 data_store_connection &store, std::vector<int> &clients_to_close, std::mutex &clients_to_close_mux,
//					 int clients_to_close_control)
void service_thread(std::queue<conn_meta *> &q, std::mutex &q_mutex, std::condition_variable &cv, 
					 SSL_CTX *ctx, std::atomic_int &connections, Config *config, 
					 std::map<std::string, User_info> & infos, fd_set & connections_fd, 
					 std::mutex &connections_fd_mutex, int control, std::atomic_int & active_threads, 
					 data_store_connection &store, std::atomic_bool & read_fds_called)
{
	std::thread::id id = std::this_thread::get_id();
	std::stringstream ss;
	ss << id;
	std::string id_str = ss.str();
	
	std::string t{"service_thread_"};
	t += id_str;
	const char *t_id = t.c_str();
	
	sync_handler handler{t, config, infos, store};
	config_http configHttp;
	
//	SSL *ssl;
	conn_meta *client;
	char control_buf[1];
	
//	auto sec = std::chrono::seconds(30);
	
	while (true) {
//		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		
		{
			std::unique_lock<std::mutex> lock{q_mutex};

			if (q.size() < 1)
				cv.wait(lock, [&] {
					return q.size() > 0;
				});
			
			active_threads++;
			
			if (q.front()->last_used ==  END_IT) {
				printf("Thread %s shutting down\n", t_id);
				q.pop();
				break;
			} else {
				client = q.front();
//				printf(" value: %d\n", client);
				q.pop();
			}
			
			printf(">>%s going to work for client fd: %d\n", t_id, client->socket);
		}
		
		
		bool ssl_errros{false};
		
		if (!client->ssl) {
			printf("%s New SSL session needed for fd: %d\n", t_id, client->socket);
			client->ssl = SSL_new(ctx);
			SSL_set_fd(client->ssl, client->socket);

			if ( SSL_accept(client->ssl) <= 0 )  {   /* do SSL-protocol accept/handshake */
				printf("Thread %s Error doing SSL handshake.\n", t_id);
				ssl_errros = true;
				ERR_print_errors_fp(stderr);
				// dont add client socket back to master FDset, let cleaner thread remove the connection
//				close_client(client, clients_to_close_mux, connections, clients_to_close, clients_to_close_control, control_buf);
				close_client(client, connections);
				active_threads--;
				continue;
			}
		}
		
		if (!ssl_errros) {
			std::string request{};
			std::string header{};
			std::string operation{};
			std::string contentLength{};
			request_type requestType;
//			char buf[1024] = {0};
//			int bytes, total{0};
			
			if (read_incoming_bytes(client->ssl, header, 0)) {
//				bool valid{true};
				printf("%s http header bytes read: %lu, header\n%s\n", t_id, header.length(), header.c_str());

				const char * reply;

				try {
					if (parse_header(header, operation, contentLength, requestType)) {

						if (requestType ==  request_type::POST) {
							//check if json is included with headers - Revisit to make a much better parser
							int content_length = std::stoi(contentLength);
							
							if (content_length < 2)
								throw register_server_exception(configHttp.build_reply(HTTP_400, close_con));

							std::string::size_type loc = header.find("{", 0);
							if (loc != std::string::npos) {
								//assuming { would never should up in any header???
								request = header.substr(loc, std::string::npos-loc);
							} else {
								read_incoming_bytes(client->ssl, request, content_length);
							}
							
							if (request.size() < 2)
								throw register_server_exception(configHttp.build_reply(HTTP_400, close_con));
							
							std::string replyMsg = handler.handle_request(operation, request, requestType);
							reply = configHttp.build_reply(HTTP_200, keep_alive, replyMsg);
						} else if (requestType == request_type::GET) {
							std::string replyMsg = handler.handle_request(operation, request, requestType);
							reply = configHttp.build_reply(HTTP_200, keep_alive, replyMsg);
						} else {
//							reply = configHttp.build_reply(HTTP_400, close_con);
							throw register_server_exception(configHttp.build_reply(HTTP_400, close_con));
						}
						
						int written = SSL_write(client->ssl, reply, strlen(reply));
						printf("%s Bytes written: %d,\n reply:\n%s\n", t_id, written, reply);
						free((char*)reply);
					}
				} catch (register_server_exception &ex) {
					printf("\n%s: Error:\nHeader\n%s\nRequest\n%s\nError\n%s\n", t_id, 
							header.c_str(), request.c_str(), ex.what());
					SSL_write(client->ssl, ex.what(), strlen(ex.what()));
//					close_client(client, clients_to_close_mux, connections, clients_to_close, clients_to_close_control, control_buf);
					close_client(client, connections);
					active_threads--;
					continue;
				}
			} else {
				printf("%s: Error trying to read header, no bytes read, closing client\n", t_id);
//				close_client(client, clients_to_close_mux, connections, clients_to_close, clients_to_close_control, control_buf);
				close_client(client, connections);
				active_threads--;
				continue;
			}
		}

		printf("%s Adding client %d back to connections_fd\n", t_id, client->socket);
		client->last_used = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		client->active = false;
		active_threads--;
		
		connections_fd_mutex.lock();
		FD_SET(client->socket, &connections_fd);
		
if (!read_fds_called) {
	read_fds_called = true;
	write(control, control_buf, 1);
}
		
		connections_fd_mutex.unlock();
		
//		write(control, control_buf, 1);
		
	}
	
}


bool verify_request(std::string &method, std::string &operation)
{
	if (method == HTTP_POST) {
		if (operation == REGISTER_CONFIG)
			return true;
		else if (operation == DELETE_USER)
			return true;
		else if (operation == SYNC_INITIAL)
			return true;
		else if (operation == SYNC_FINAL)
			return true;
		else {
			config_http configHttp;
			throw register_server_exception{configHttp.build_reply(HTTP_400, close_con)};
		}
	} else if (method == HTTP_GET && operation == REGISTER_CONFIG) {
		return true;
	} else {
		config_http configHttp;
		throw register_server_exception{configHttp.build_reply(HTTP_400, close_con)};
	}
}


bool read_incoming_bytes(SSL *ssl, std::string &msg, int contentLength)
{
	char buf[1024] = {0};
	int bytes, total{0};

	do {
		bytes = SSL_read(ssl, buf, sizeof(buf));
		buf[bytes] = '\0';
		msg.append(buf);
		total += bytes;
		printf("bytes read: %d\n", bytes);
	} while ((total < contentLength || SSL_has_pending(ssl)/*&& bytes == 1024*/) && bytes != -1); 
	// could be an issue if a header happens to be exactly 1024 since the length is not
	// known. may look at better solution.
	
	if (total < 1) 
		return false;
	else
		return true;
	
}


bool parse_header(std::string &header, std::string &operation, std::string &contentLength, request_type &requestType)
{
	// get method 
	std::string resource = header.substr(0, header.find("\n", 0));
	std::string::size_type loc = resource.find(" ", 0);
				
	if (loc == std::string::npos) 
		return false;
		
	std::string method = resource.substr(0, loc);

  //get operation
	std::string::size_type loc2 = resource.find(" ", loc+1);

	if (loc2 == std::string::npos) 
		return false;

	operation = resource.substr(loc+1, loc2-(loc+1));

	if (!verify_request(method, operation))
		return false;

	if (method == HTTP_GET) {
		requestType = request_type::GET;
	} else if (method == HTTP_POST) {	
		requestType = request_type::POST;
		loc = header.find(HTTP_CONTENT_LENGTH_UPPER);

		if (loc == std::string::npos)
			loc = header.find(HTTP_CONTENT_LENGTH_LOWER);
			if (loc == std::string::npos)
				return false;

		loc2 = header.find("\n", loc);
		contentLength = header.substr(loc+16, loc2-loc+16);
	}
	
	return true;
}


//void close_client(conn_meta *client, std::mutex &clients_to_close_mux, std::atomic_int &connections, 
//					std::vector<int> &clients_to_close, int clients_to_close_control, char *control_buf)
void close_client(conn_meta *client, std::atomic_int &connections)
{
int s = client->socket;
client->socket = -1;
	SSL_free(client->ssl);
	close(s);
//	client->socket = -1;
	connections--;
	
//	client->active = false;
//	client->last_used = 1;
//	clients_to_close_mux.lock();
//	clients_to_close.push_back(client->socket);
//	clients_to_close_mux.unlock();
//	write(clients_to_close_control, control_buf, 1);
}