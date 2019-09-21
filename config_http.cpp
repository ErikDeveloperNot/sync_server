#include "config_http.h"
#include <cstring>

const char* config_http::build_reply(http_status status, http_connection con) 
{
	std::string s;
	return build_reply(status, con, s);
}


const char* config_http::build_reply(http_status status, http_connection con, std::string & msg)
{
	std::string reply, m;
	m = msg;
	
	switch (status)
	{
		case HTTP_200 :
			reply = "HTTP/1.1 200 OK\r\n";
//			m = msg;
			break;
		case HTTP_400 :
			reply = "HTTP/1.1 400 Bad Request\r\n";
			
			if (msg.length() < 1)
				m = "{ error: \"Bad Request\" }";
			break;
		case HTTP_403 :
			reply = "HTTP/1.1 403 Forbidden\r\n";
			
			
			if (msg.length() < 1)
				m = "{ error: \"Unauthorized\" }";
			break;	
		case HTTP_500 :
			reply = "HTTP/1.1 500 Internal Server Error\r\n";
			
			if (msg.length() < 1)
				m = "{ error: \"Internal Server Error\" }";
			break;
		case HTTP_503 :
			reply = "HTTP/1.1 503 Service Unavailable\r\n";
			
			if (msg.length() < 1)
				m = "{ error: \"Server Busy\" }";
			break;
	}
	
	reply += http_content_type;
	
	switch (con)
	{
		case close_con :
			reply += http_connection_close;
		break;
		case keep_alive :
			reply += http_connection_keep_alive;
		break;
	}
	
	reply += http_content_length;
	reply += std::to_string(m.length());
	reply += "\r\n\r\n";
	reply += m;
	
	char * tmp = (char *) malloc((reply.length()+1) * sizeof(char));
	strcpy(tmp, reply.c_str());
	return tmp;
}
