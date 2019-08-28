#ifndef __CONFIG_HTTP_H__
#define __CONFIG_HTTP_H__
#include <string>



std::string STATUS_200 = "HTTP/1.1 200 \r\nnConnection: close\r\nContent-Length: ";
std::string STATUS_200_INITIAL_SYNC = "HTTP/1.1 200 \r\nConnection: keep-alive\r\nKeep-Alive: timeout=10\r\nContent-Length: ";

char * STATUS_400 = "HTTP/1.1 400 OK\nConnection: close\n\nBad Request\n\nBad Request";
char * STATUS_401 = "HTTP/1.1 401 OK\nConnection: close\n\nUnauthorized";
char * STATUS_500_SERVER_ERROR = "HTTP/1.1 500 OK\nConnection: close\n\nInternal Server Error\n\nServer Error";
char * STATUS_500_USER_EXISTS = "HTTP/1.1 500 OK\nConnection: close\n\nUser Already Exists";
char * STATUS_503 = "HTTP/1.1 503 OK\nConnection: close\n\nServer Busy";


const std::string HTTP_GET = "GET";
const std::string HTTP_POST = "POST";
const std::string HTTP_CONTENT_LENGTH = "Content-Length:";

//enum http_status = {200, 400, 401, 500, 503};


#endif // __CONFIG_HTTP_H__