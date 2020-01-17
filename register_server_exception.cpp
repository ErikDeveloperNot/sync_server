#include "register_server_exception.h"
#include <cstring>

register_server_exception::register_server_exception(char* msg)  
{
	copy_message(msg);
//	printf("RegisterExeption 1\n");
}

register_server_exception::register_server_exception(const char* msg) 
{
//	message = (char*)msg;
	copy_message((char*)msg);
//	printf("RegisterExeption 2\n");
}

const char* register_server_exception::register_server_exception::what() const noexcept {
	return message;
}


register_server_exception::register_server_exception(const register_server_exception & other) 
{
//	message = (char*) malloc(strlen(other.what())+1 * sizeof(char));
//	message = strcpy(message, other.what());
//	message = other.message;
	copy_message(other.message);
//	printf("RegisterExeption 3\n");
}

register_server_exception::register_server_exception(const register_server_exception && other) 
{
//	message = (char*) malloc(strlen(other.what())+1 * sizeof(char));
//	message = strcpy(message, other.what());
//	message = other.message;
	copy_message(other.message);
//	printf("RegisterExeption 4\n");
}

register_server_exception::~register_server_exception()
{
//	if (copyContructed) {
//		printf("Deleting Malloc message......\n\n\n");
//		free(message);
//	} else {
//		printf("NOT Deleting Malloc message......\n\n\n");
//	}
	free(message);
}

void register_server_exception::copy_message(char* msg)
{
	message = (char*) malloc(strlen(msg)+1 * sizeof(char));
	message = strcpy(message, msg);
}
