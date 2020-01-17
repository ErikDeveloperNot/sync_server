#include "config_http.h"
#include <cstring>

char* config_http::build_reply(http_status status, http_connection con) 
{
//	char *s;
	return build_reply(status, con, NULL);
}


char* config_http::build_reply(http_status status, http_connection con, char *msg)
{
	size_t msg_l{0};

	if (msg != NULL)
		msg_l = strlen(msg);
	
	char *reply;
	unsigned int reply_i{0};
	
	if (msg_l > 0) {
		reply = (char*)malloc(msg_size + msg_l);
	} else {
		reply = (char*)malloc(msg_size + 40);
	}

	switch (status)
	{
		case HTTP_200 :
			strcpy(reply, HTTP_200_HDR);
			reply_i += strlen(HTTP_200_HDR);
			break;
		case HTTP_400 :
			strcpy(reply, HTTP_400_HDR);
			reply_i += strlen(HTTP_400_HDR);
			
			if (msg_l < 1)
				msg = (char*)HTTP_400_MSG;
			break;
		case HTTP_403 :
			strcpy(reply, HTTP_403_HDR);
			reply_i += strlen(HTTP_403_HDR);
			
			if (msg_l < 1)
				msg = (char*)HTTP_403_MSG;
			break;	
		case HTTP_500 :
			strcpy(reply, HTTP_500_HDR);
			reply_i += strlen(HTTP_500_HDR);
			
			if (msg_l < 1)
				msg = (char*)HTTP_500_MSG;
			break;
		case HTTP_503 :
			strcpy(reply, HTTP_503_HDR);
			reply_i += strlen(HTTP_503_HDR);
			
			if (msg_l < 1)
				msg = (char*)HTTP_503_MSG;
			break;
	}
	
	strcpy(&reply[reply_i], http_content_type);
	reply_i += strlen(http_content_type);
	
	switch (con)
	{
		case close_con :
			strcpy(&reply[reply_i], http_connection_close);
			reply_i += strlen(http_connection_close);
			break;
		case keep_alive :
			strcpy(&reply[reply_i], http_connection_keep_alive);
			reply_i += strlen(http_connection_keep_alive);
			break;
	}
	
	strcpy(&reply[reply_i], http_content_length);
	reply_i += strlen(http_content_length);
	reply_i += sprintf(&reply[reply_i], "%u", (unsigned int)strlen(msg));
	strcpy(&reply[reply_i], "\r\n\r\n");
	reply_i += 4;
	strcpy(&reply[reply_i], msg);
	
	return reply;
}
