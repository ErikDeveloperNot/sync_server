#ifndef __REGISTER_SERVER_EXCEPTION_H__
#define __REGISTER_SERVER_EXCEPTION_H__
#include "config_http.h"
#include <exception>
//#include <string>



class register_server_exception : public std::exception
{
public:
	register_server_exception(char* message);
	register_server_exception(const char* message);
	register_server_exception(const register_server_exception&);
    register_server_exception(const register_server_exception&&);
    
	~register_server_exception();
	
	virtual const char* what() const noexcept;
	
private:
	char* message;
	
	void copy_message(char *);
};


#endif // __REGISTER_SERVER_EXCEPTION_H__
