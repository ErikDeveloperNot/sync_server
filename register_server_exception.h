#ifndef __REGISTER_SERVER_EXCEPTION_H__
#define __REGISTER_SERVER_EXCEPTION_H__
#include <exception>
//#include <string>



class register_server_exception : public std::exception
{
public:
	register_server_exception(char* message);
    ~register_server_exception() = default;
	
	virtual const char* what() const noexcept;
	
private:
	char* message;
};


#endif // __REGISTER_SERVER_EXCEPTION_H__
