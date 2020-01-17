#ifndef __CONFIG_HTTP_H__
#define __CONFIG_HTTP_H__
#include <string>


//const std::string STATUS_200 = "HTTP/1.1 200 \r\nnConnection: close\r\nContent-Length: ";
//const std::string STATUS_200_INITIAL_SYNC = "HTTP/1.1 200 \r\nConnection: keep-alive\r\nKeep-Alive: timeout=10\r\nContent-Length: ";

//char * STATUS_400 = "HTTP/1.1 400 OK\nConnection: close\n\nBad Request\n\nBad Request";
//char * STATUS_401 = "HTTP/1.1 401 OK\nConnection: close\n\nUnauthorized";
//char * STATUS_500_SERVER_ERROR = "HTTP/1.1 500 \nContent-Type: application/json\nConnection: close\nContent-Length: 34\n\n{ error: \"Internal Server Error\" }";
//char * STATUS_500_USER_EXISTS = "HTTP/1.1 500 OK\nConnection: close\n\nUser Already Exists";
//std::string STATUS_503 = "HTTP/1.1 503 OK\nConnection: close\n\nServer Busy";

static const char HTTP_200_HDR[] = {"HTTP/1.1 200 OK\r\n"};
static const char HTTP_400_HDR[] = {"HTTP/1.1 400 Bad Request\r\n"};
static const char HTTP_400_MSG[] = {"{ error: \"Bad Request\" }"};
static const char HTTP_403_HDR[] = {"HTTP/1.1 403 Forbidden\r\n"};
static const char HTTP_403_MSG[] = {"{ error: \"Unauthorized\" }"};
static const char HTTP_500_HDR[] = {"HTTP/1.1 500 Internal Server Error\r\n"};
static const char HTTP_500_MSG[] = {"{ error: \"Internal Server Error\" }"};
static const char HTTP_503_HDR[] = {"HTTP/1.1 503 Service Unavailable\r\n"};
static const char HTTP_503_MSG[] = {"{ error: \"Server Busy\" }"};

static const char HTTP_GET[] = {"GET"};
static const char HTTP_POST[] = {"POST"};
static const char HTTP_CONTENT_LENGTH_UPPER[] = {"Content-Length:"};
static const char HTTP_CONTENT_LENGTH_LOWER[] = {"content-length:"};
static const char  http_connection_close[] = {"Connection: close\r\n"};
static const char  http_connection_keep_alive[] = {"Connection: keep-alive\r\n"};
static const char  http_content_type[] = {"Content-Type: application/json\r\n"};
static const char  http_content_length[] = {"Content-Length: "};


//429 is used for too many requests for the same account
enum http_status : int {HTTP_200, HTTP_400, HTTP_403, HTTP_429, HTTP_500, HTTP_503};
//enum http_message : int {HTTP_200, HTTP_400, HTTP_401, HTTP_500_SERVER_ERROR, HTTP_500_USER_EXIST, HTTP_503};
enum http_connection : int {keep_alive, close_con};

const unsigned int msg_size = 166 * sizeof(char);

class config_http
{
private:
	
	
public:
	config_http() = default;
	~config_http() = default;
	
	char * build_reply(http_status status, http_connection con, char *msg);
	char * build_reply(http_status status, http_connection con);
};


#endif // __CONFIG_HTTP_H__