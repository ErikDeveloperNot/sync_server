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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
 * v2:
 * will keep connection open from server side and each thread will be in charge
 * of its own fd sets and run its own select, or some other type of solution like libevent.
 * might revisit individual locks on map associated with each user account
 */


void service_thread(std::queue<int> &q, std::mutex &q_mutex, std::condition_variable &cv, 
					 SSL_CTX *ctx, const int id, std::atomic_int &connections, Config *config, 
					 std::map<std::string, User_info> &);


server::server(Config *config)
{
	int listen_sd = startListener(config);
	this->config = config;
//	std::set<User_info, compare> *infos = new std::set<User_info, compare>{};
	std::map<std::string, User_info> infos;// = new std::map<std::string, User_info>{};
	
	for (int i{1}; i<=config->getServiceThreads(); i++) {
		std::thread t{service_thread, std::ref(service_q), std::ref(service_mutex), 
						std::ref(cv), ctx, i, std::ref(current_connections), config, std::ref(infos)};
		t.detach();
		printf("Thread: %d, started\n", i);
	}
	
	
	while (true) {
//		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		struct sockaddr_in addr;
		socklen_t len = sizeof(addr);
		
		int client = accept(listen_sd, (struct sockaddr*)&addr, &len);
		printf("Connection: %s:%d\n",inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
		
		if (current_connections >= config->getServerMaxConnections()) {
			printf("Max Connections %d reached, rejecting\n", static_cast<int>(current_connections));
			handleServerBusy(client);
			continue;
		}
		
		current_connections++;
		std::lock_guard<std::mutex> lg{service_mutex};
		service_q.push(client);
		cv.notify_one();
		
	}
}


server::~server()
{
	printf("Server shutting down, waiting for threads to complete\n");
	
	for (size_t i{0}; i<config->getServiceThreads(); i++) {
		std::lock_guard<std::mutex> lg{service_mutex};
		service_q.push(END_IT);
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
		SSL_write(ssl, STATUS_500_SERVER_ERROR, strlen(STATUS_500_SERVER_ERROR));
		SSL_free(ssl);         /* release SSL state */
		close(client);
	}
}


// functions used by service thread
bool verify_request(std::string &method, std::string &operation);
bool read_incoming_bytes(SSL *, std::string &msg, int contentLenth);
bool parse_header(std::string &header, std::string &operation, std::string &contentLenth, request_type &requestType);

// definition of service thread-
void service_thread(std::queue<int> &q, std::mutex &q_mutex, std::condition_variable &cv, 
					 SSL_CTX *ctx, const int id, std::atomic_int &connections, Config *config, 
					 std::map<std::string, User_info> & infos)
{
	std::string t{"service_thread_"};
	t += std::to_string(id);
	const char *t_id = t.c_str();
	
	sync_handler handler{t, config, infos};
	
//	std::cout << t_id << std::endl;
	
	SSL *ssl;
	
	while (true) {
//		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		int client;
		
		{
			std::unique_lock<std::mutex> lock{q_mutex};
			
			cv.wait(lock, [&] {
				return q.size() > 0;
			});
			
			printf(">>%s going to work\n", t_id);

			if (q.front() ==  END_IT) {
				printf("Thread %s shutting down\n", t_id);
				q.pop();
				break;
			} else {
				client = q.front();
				printf(" value: %d\n", client);
				q.pop();
			}
		}
		
		ssl = SSL_new(ctx);
		SSL_set_fd(ssl, client); 
		
		if ( SSL_accept(ssl) <= 0 )  {   /* do SSL-protocol accept/handshake */
			ERR_print_errors_fp(stderr);
		} else {
			std::string request{};
			std::string header{};
			std::string operation{};
			std::string contentLength{};
			request_type requestType;
			char buf[1024] = {0};
			int bytes, total{0};
			
			if (read_incoming_bytes(ssl, header, 0)) {
				bool valid{true};
				printf("http header bytes read: %d, header\n%s\n", header.length(), header.c_str());

				std::string reply;

				try {
					if (parse_header(header, operation, contentLength, requestType)) {
						if (requestType ==  request_type::POST && read_incoming_bytes(ssl, request, std::stoi(contentLength))) {
							reply = STATUS_200 +
								handler.handle_request(operation, request, requestType);
							
							printf("1\n");
						} else if (requestType == request_type::GET) {
							reply = STATUS_200 + 
								handler.handle_request(operation, request, requestType);
						} else {
							reply = STATUS_400;
						}
						
						int written = SSL_write(ssl, reply.c_str(), reply.length());
						printf("Bytes written: %d\n", written);
					}
				} catch (register_server_exception &ex) {
					printf("\n%s: Error:\nHeader\n%s\nRequest\n%s\nError: %s\n", t_id, 
							header.c_str(), request.c_str(), ex.what());
					SSL_write(ssl, ex.what(), strlen(ex.what()));
				}
			}
		}
		
		SSL_free(ssl);         /* release SSL state */
		close(client);
		connections--;
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
//	printf("-%s-\n", method.c_str());
				
	//get operation
	std::string::size_type loc2 = resource.find(" ", loc+1);
//	printf(".5\n");
	if (loc2 == std::string::npos) 
		return false;
//	printf("1\n");
	operation = resource.substr(loc+1, loc2-(loc+1));
//	printf("2\n");						
	if (!verify_request(method, operation))
		return false;
//	printf("3\n");	
	if (method == HTTP_GET) {
		requestType = request_type::GET;
//		contentLength = "-1";
	} else if (method == HTTP_POST) {	
		requestType = request_type::POST;
		loc = header.find(HTTP_CONTENT_LENGTH);
				 
		if (loc == std::string::npos)
			return false;
//	printf("4\n");
		loc2 = header.find("\n", loc);
		contentLength = header.substr(loc+16, loc2-loc+16);
//		printf("length: %s\n", contentLength.c_str());
	}
	
	return true;
}