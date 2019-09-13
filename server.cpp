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
	
	listen_sd = startListener(config);
	max_fd = listen_sd;
	FD_SET(listen_sd, &connection_fds);
	
	printf("\n\n");
	printf("Server fd: %d, max connections: %d\n", listen_sd, max_connections);
	printf("control read fd: %d\n", control[0]);
	printf("control write fd: %d\n", control[1]);
	
	current_thread_count = 0;

	for (int i{1}; i<=config->getServiceThreads(); i++) {
		start_service_thread();
	}
	
	start_service_thread_manager();
	
	if (config->isServerUseKeepAliveCleaner())
		start_client_seesion_manager();
}


void server::start()
{
//	struct timeval tv;
//	int keep_alive_secs = config->getServerKeepAlive();
//	tv.tv_sec = keep_alive_secs;
//	tv.tv_usec = 0;
//	int keep_alive = config->getServerKeepAlive();
//	bool use_keep_alive = config->isServerUseKeepAliveCleaner();
//	last_thread_check = current_time_sec();
//	thread_check_counter = 0;
		
		while (true) {
connection_fds_mutex.lock();
		read_fds = connection_fds;
connection_fds_mutex.unlock();
		served = 0;

//		printf("Entering SELECT\n");
		
//		if (use_keep_alive) {
//			tv.tv_sec = keep_alive_secs;
//			tv.tv_usec = 0;
//			rv = select(max_fd+1, &read_fds, NULL, NULL, &tv);
//		} else {
			rv = select(max_fd+1, &read_fds, NULL, NULL, NULL);
//		}
		
			
		printf("SELECT returned: %d\n", rv);
printf("queue size: %d\n", service_q.size());

		if (rv < 0) {
			//error
			printf("Error received doing select(), try to find offending fd\n");
			perror("select");
			
		} else if (rv == 0) {
			// clean old sessions if enabled
//			close_old_connections();
//			check_threads();
		} else {
			printf("max_fd = %d\n", max_fd);
			if (FD_ISSET(control[0], &read_fds)) {
				printf(">>>>>>>>>>>>> CONTROL %d was modified\n", control[0]);
				fgets(control_buf, 2, control_file);
//				printf("Done fgets, %s\n", control_buf);
				
				if (rv == 1)
					continue;
				else
					served++;
				//continue;
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

			long long now = current_time_ms();
			
			for (int i=0; i<=max_fd; i++) {
				if (rv == served) {
					//all active fd handled, break
					break;
				}
				
				if (i == control[0] || i == listen_sd)
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
				} /*else {
					// clean old sessions if enabled
					if (use_keep_alive) {
						if (conn_map.count(i) > 0) {
							if (now - conn_map[i].last_used > keep_alive && !conn_map[i].active) {
								close_client(i, conn_map[i].ssl, connection_fds, connection_fds_mutex);
								current_connections--;
								conn_map.erase(i);
							}
						}
					}
				}*/
//				std::this_thread::sleep_for(std::chrono::milliseconds(200));
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
//	current_connections--;
//	conn_map.erase(i);
}


void server::start_client_seesion_manager()
{
	printf("Starting client session manager thread\n");
	
	std::thread manager([&]() {
		
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


void server::close_old_connections()
{
	long now = current_time_sec();
	int keep_alive = config->getServerKeepAlive();
			
//	for (int i{0}; i <=max_fd; i++) {
//		if (conn_map.count(i) > 0) {
//			if (now - conn_map[i].last_used > keep_alive && !conn_map[i].active) {
//				close_client(i, conn_map[i].ssl, connection_fds, connection_fds_mutex);
//				current_connections--;
//				conn_map.erase(i);
//			}
//		}
//	}
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
//	current_thread_count = thread_id;
	current_thread_count++;
	
	std::thread t{service_thread, std::ref(service_q), std::ref(service_mutex), 
						std::ref(cv), ctx, std::ref(current_connections), config, std::ref(infos),
						std::ref(connection_fds), std::ref(conn_map), std::ref(connection_fds_mutex),
						control[1]};
	
//	service_threads.push_back(std::move(t));
		
//	if (extra)
//		printf("Adding temporary Thread: %d, started\n", thread_id);
//	else
		printf("Thread: %d, started, current thread count: %d\n", t.get_id(), current_thread_count);
		t.detach();
}


void server::start_service_thread_manager()
{
	// TODO - revisit, instead use a circular int array to keep track of q size, 
	std::thread manager([&]() {
		int server_threads = config->getServiceThreads();
		int prior_q_sz{0};
		int thread_check_counter{0};
		long last_thread_removal = current_time_sec();
		
		while (true) {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			long now = current_time_sec();
			int current_q_sz = service_q.size();
if (current_q_sz > 10)
printf("thread manager:: current q sz:%d, prior: %d, thread_check_counter:%d\n", current_q_sz, prior_q_sz, thread_check_counter);
			if (current_q_sz < 1) {
//				if (--thread_check_counter < 0)
					thread_check_counter = 0;
					
				if (current_thread_count > server_threads && now - last_thread_removal > 120) {
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
					
					last_thread_removal = now;
				}
			} else if (current_q_sz > current_thread_count/1.5 ||
						(current_q_sz >= prior_q_sz && current_q_sz > 0)) {
				if (++thread_check_counter >= 3) {
					thread_check_counter = 3;
					
					for (size_t j{0}; j < current_q_sz/2; j++) {
						start_service_thread();
						
						if (current_thread_count >= server_threads)
							break;
					}
				}
			} else {
				if (--thread_check_counter < 0)
					thread_check_counter = 0;
			}
			
			prior_q_sz = current_q_sz;
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

// definition of service thread-
void service_thread(std::queue<conn_meta *> &q, std::mutex &q_mutex, std::condition_variable &cv, 
					 SSL_CTX *ctx, std::atomic_int &connections, Config *config, 
					 std::map<std::string, User_info> & infos, fd_set & connections_fd, 
					 std::map<int, conn_meta> & conn_map, std::mutex &connections_fd_mutex,
					 int control)
{
	std::thread::id id = std::this_thread::get_id();
	std::stringstream ss;
	ss << id;
	std::string id_str = ss.str();
	
	std::string t{"service_thread_"};
	t += id_str;
	const char *t_id = t.c_str();
	
	sync_handler handler{t, config, infos};
	config_http configHttp;
	
	SSL *ssl;
	conn_meta *client;
	char control_buf[1];
	
//	auto sec = std::chrono::seconds(30);
	
	while (true) {
//		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		
		{
			std::unique_lock<std::mutex> lock{q_mutex};
			
//			if (!extra) {
//				cv.wait(lock, [&] {
//					return q.size() > 0;
//				});
//			} else { 
//				cv.wait_for(lock, sec, [&] {
//					return (q.size() > 0 || running == 0);
//				});
//				
//				if (running == 0) {
//					printf("Thread %s shutting down\n", t_id);
//					break;
//				}
//					
//			}

if (q.size() < 1)
			cv.wait(lock, [&] {
				return q.size() > 0;
			});
			
			printf(">>%s going to work\n", t_id);

			if (q.front()->last_used ==  END_IT) {
				printf("Thread %s shutting down\n", t_id);
				q.pop();
				break;
			} else {
				client = q.front();
//				printf(" value: %d\n", client);
				q.pop();
			}
		}
		
		
		bool ssl_errros{false};
		
		if (!client->ssl) {
printf("New SSL session needed\n");
			client->ssl = SSL_new(ctx);
			SSL_set_fd(client->ssl, client->socket);

			if ( SSL_accept(client->ssl) <= 0 )  {   /* do SSL-protocol accept/handshake */
				printf("Thread %s Error doing SSL handshake.\n", t_id);
				ssl_errros = true;
				ERR_print_errors_fp(stderr);
			}
		}
		
		if (!ssl_errros) {
			std::string request{};
			std::string header{};
			std::string operation{};
			std::string contentLength{};
			request_type requestType;
			char buf[1024] = {0};
			int bytes, total{0};
			
			if (read_incoming_bytes(client->ssl, header, 0)) {
				bool valid{true};
				printf("http header bytes read: %d, header\n%s\n", header.length(), header.c_str());

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
							reply = configHttp.build_reply(HTTP_400, close_con);
						}
						
						int written = SSL_write(client->ssl, reply, strlen(reply));
						printf("Bytes written: %d\n, reply:\n%s\n", written, reply);
						free((char*)reply);
					}
				} catch (register_server_exception &ex) {
					printf("\n%s: Error:\nHeader\n%s\nRequest\n%s\nError\n%s\n", t_id, 
							header.c_str(), request.c_str(), ex.what());
					SSL_write(client->ssl, ex.what(), strlen(ex.what()));
				}
			}
		}

printf(">> Adding client %d back to connections_fd\n", client->socket);
		client->last_used = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		client->active = false;
connections_fd_mutex.lock();
		FD_SET(client->socket, &connections_fd);
connections_fd_mutex.unlock();
		write(control, control_buf, 1);
		
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
	} while (total < contentLength || bytes == 1024); 
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
