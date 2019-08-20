#ifndef __JSON_PARSER_EXCEPTION_H__
#define __JSON_PARSER_EXCEPTION_H__

#include <exception>


class json_parser_exception : public std::exception
{
public:
	json_parser_exception(char*);
	~json_parser_exception() = default;

	virtual const char* what() const noexcept;
	
private:
	char* message;
};

#endif // __JSON_PARSER_EXCEPTION_H__
