#include "server.h"
#include "Config.h"
#include "sync_handler.h"
#include "config_http.h"
#include "register_server_exception.h"

#include <thread>
#include <iostream>
#include <atomic>
#include <string>
#include <string.h>
#include <map>
#include <vector>

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




void service_thread(std::queue<conn_meta *> &q, std::mutex &q_mutex, std::condition_variable &cv, 
					 SSL_CTX *ctx, const int id, std::atomic_int &connections, Config *config, 
					 std::map<std::string, User_info> &, fd_set &, std::map<int, conn_meta> &,
					 std::mutex &fd_set_mutex, int);
					 
void session_cleaner_thread(std::atomic_int &connections, int keep_alive, fd_set &connection_fds,
							std::mutex &connections_fds_mutex, std::map<int, conn_meta> &, int, int &);


server::server(Config *config)
{
	int listen_sd = startListener(config);
	this->config = config;
	std::map<std::string, User_info> infos;
	std::map<int, conn_meta> conn_map;
	
	fd_set connection_fds;
	fd_set read_fds;
	std::mutex connection_fds_mutex;
	FD_ZERO(&connection_fds);
	FD_ZERO(&read_fds);
	int max_fd = listen_sd;
	FD_SET(listen_sd, &connection_fds);
	int rv;
	int served;
	
	//used for worker threads to wake up main from select call
	int control[2];
	pipe(control);
	char control_buf[10];
	FD_SET(control[0], &connection_fds);
	FILE *control_file = fdopen( control[0], "r" );
	
	//used for eol session cleaner thread
	int control_keep_alive[2];
	pipe(control_keep_alive);
	char control_keep_alive_buf[10];
	FD_SET(control_keep_alive[0], &connection_fds);
	FILE *control_keep_alive_file = fdopen(control_keep_alive[0], "r");
	
	printf("\n\n");
	printf("Server fd: %d\n", listen_sd);
	printf("control read fd: %d\n", control[0]);
	printf("control write fd: %d\n", control[1]);
	printf("control keep-alive read fd: %d\n", control_keep_alive[0]);
	printf("control keep-alive write fd: %d\n\n", control_keep_alive[1]);
	
	for (int i{1}; i<=config->getServiceThreads(); i++) {
		std::thread t{service_thread, std::ref(service_q), std::ref(service_mutex), 
						std::ref(cv), ctx, i, std::ref(current_connections), config, std::ref(infos),
						std::ref(connection_fds), std::ref(conn_map), std::ref(connection_fds_mutex),
						control[1]};
		t.detach();
		printf("Thread: %d, started\n", i);
	}

	
	while (true) {
		connection_fds_mutex.lock();
		read_fds = connection_fds;
		connection_fds_mutex.unlock();
		served = 0;
		printf("Entering SELECT\n");
		rv = select(max_fd+1, &read_fds, NULL, NULL, NULL);
		printf("SELECT returned: %d\n", rv);

		if (rv < 0) {
			//error
			printf("Error received doing select(), try to find offending fd\n");
			perror("select");
			
		} else if (rv == 0) {
			//timeout - may use to look for stale connections if don't reply on default os settings
			printf("Error: Result value from select = 0, timeout is not being used?\n");
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

			for (int i=0; i<=max_fd; i++) {
				if (rv == served) {
					//all active fd handled, break
					break;
				}
				
				if (rv == control[0])
					continue;
					
//				printf("1, %d\n", i);
				if (FD_ISSET(i, &read_fds)) {
					served++;
//					printf("2\n");
					if (i == listen_sd) {
//						printf("3\n");
						//handle new connection
						struct sockaddr_in addr;
						socklen_t len = sizeof(addr);
						
						int client = accept(listen_sd, (struct sockaddr*)&addr, &len);
						printf("Connection: %s:%d, client fd:%d\n",inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), client);
						
						if (current_connections >= config->getServerMaxConnections()) {
							printf("Max Connections %d reached, rejecting\n", static_cast<int>(current_connections));
							handleServerBusy(client);
							continue;
						}

						if (client > max_fd)
							max_fd = client;
							
						conn_map[client] = conn_meta(client, NULL, 1L, true);   //revisit time if I use this field
						current_connections++;
						std::lock_guard<std::mutex> lg{service_mutex};
						service_q.push(&(conn_map[client]));
						cv.notify_one();
					} else {
						if (conn_map[i].active)		//fd is already being served
							continue;
							
						int n = 0;
						ioctl(i, FIONREAD, &n);
						
						if (n == 0) {
							printf("closing client\n");
connection_fds_mutex.lock();
							FD_CLR(i, &connection_fds);
connection_fds_mutex.unlock();

							SSL_free(conn_map[i].ssl);         /* release SSL state */
							close(i);
							printf("should be closed\n");
							current_connections--;
							conn_map.erase(i);
						} else if (n > 0) {

//TODO - revist to verify this client is not already active, if so ignore this
printf(">>Removing client %d from connections_fds\n", i);
connection_fds_mutex.lock();
							FD_CLR(i, &connection_fds);
connection_fds_mutex.unlock();
							std::lock_guard<std::mutex> lg{service_mutex};
//							service_q.push(client);
							conn_map[i].active = true;
							service_q.push(&(conn_map[i]));
							cv.notify_one();
						} else {
							printf("ERRRRRRROOOOORRRR - dont think i should see\n");
						}
					}
				}
//				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
		}
	}
}


server::~server()
{
	printf("Server shutting down, waiting for threads to complete\n");
	
	for (size_t i{0}; i<config->getServiceThreads(); i++) {
		std::lock_guard<std::mutex> lg{service_mutex};
//		service_q.push(END_IT); REVISIT
		cv.notify_one();
	}
	
	do {
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		printf("Checking if service threads are complete\n");
	} while (service_q.size() > 1);
	
	printf("Server done\n");
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
    
	if ( listen(sd, 10) != 0 )
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
//		const char *response = STATUS_503;
		SSL_write(ssl, STATUS_503, strlen(STATUS_503));
	}
	
	SSL_free(ssl);         /* release SSL state */
	close(client);

}


// functions used by service thread
bool verify_request(std::string &method, std::string &operation);
bool read_incoming_bytes(SSL *, std::string &msg, int contentLenth);
bool parse_header(std::string &header, std::string &operation, std::string &contentLenth, request_type &requestType);

// definition of service thread-
void service_thread(std::queue<conn_meta *> &q, std::mutex &q_mutex, std::condition_variable &cv, 
					 SSL_CTX *ctx, const int id, std::atomic_int &connections, Config *config, 
					 std::map<std::string, User_info> & infos, fd_set & connections_fd, 
					 std::map<int, conn_meta> & conn_map, std::mutex &connections_fd_mutex,
					 int control)
{
	std::string t{"service_thread_"};
	t += std::to_string(id);
	const char *t_id = t.c_str();
	
	sync_handler handler{t, config, infos};
	
	SSL *ssl;
	conn_meta *client;
	char control_buf[1];
	
	
	while (true) {
//		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//		int client;
		
		{
			std::unique_lock<std::mutex> lock{q_mutex};
			
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

				std::string reply;

				try {
					if (parse_header(header, operation, contentLength, requestType)) {
						if (requestType ==  request_type::POST && read_incoming_bytes(client->ssl, request, std::stoi(contentLength))) {

							if (operation == SYNC_INITIAL) {
								std::string replyMsg = handler.handle_request(operation, request, requestType);
								reply = STATUS_200_INITIAL_SYNC + std::to_string(replyMsg.length()) + "\r\n\r\n" + replyMsg;
							} else {
								std::string replyMsg = handler.handle_request(operation, request, requestType);
								reply = STATUS_200 + std::to_string(replyMsg.length()) + "\r\n\r\n" + replyMsg;
//								reply = STATUS_200 + handler.handle_request(operation, request, requestType);
							}

						} else if (requestType == request_type::GET) {
							reply = STATUS_200 + 
								handler.handle_request(operation, request, requestType);
						} else {
							reply = STATUS_400;
						}
						
						int written = SSL_write(client->ssl, reply.c_str(), reply.length());
//						client->active = false;
						printf("Bytes written: %d\n, reply:\n%s\n", written, reply.c_str());

					}
				} catch (register_server_exception &ex) {
					printf("\n%s: Error:\nHeader\n%s\nRequest\n%s\nError: %s\n", t_id, 
							header.c_str(), request.c_str(), ex.what());
					SSL_write(client->ssl, ex.what(), strlen(ex.what()));
				}
			}
		}

printf(">> Adding client %d back to connections_fd\n", client->socket);
		connections_fd_mutex.lock();
		FD_SET(client->socket, &connections_fd);
		client->active = false;
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
		else
			throw register_server_exception{STATUS_400};
	} else if (method == HTTP_GET && operation == REGISTER_CONFIG) {
		return true;
	} else {
		throw register_server_exception{STATUS_400};
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
		loc = header.find(HTTP_CONTENT_LENGTH);
				 
		if (loc == std::string::npos)
			return false;

		loc2 = header.find("\n", loc);
		contentLength = header.substr(loc+16, loc2-loc+16);
	}
	
	return true;
}


void session_cleaner_thread(std::atomic_int &connections, int keep_alive, fd_set &connection_fds,
							std::mutex &connections_fds_mutex, std::map<int, conn_meta> &client_connections, 
							int control_fd, int &max_fd)
{
	//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	printf("keep alive session thread starting, thread interval: %d seconds\n", keep_alive/2);
	int sleep_interval = (keep_alive/2) * 1000;
	std::vector<int> to_close;
	
	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(sleep_interval));
		int checked = 0;
		int size = client_connections.size();
		
		for (int i=0; i<max_fd && checked<size; i++) {
			if (client_connections.count(i) > 0) {
				
			}
		}
	}
}