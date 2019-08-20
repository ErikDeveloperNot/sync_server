#include "json_parser_exception.h"

json_parser_exception::json_parser_exception(char *message) : message{message}
{
}

const char* json_parser_exception::what() const noexcept {
	return message;
}