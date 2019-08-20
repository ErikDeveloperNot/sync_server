#include "register_server_exception.h"

register_server_exception::register_server_exception(char* message) : message{message} {}

const char* register_server_exception::register_server_exception::what() const noexcept {
	return message;
}
