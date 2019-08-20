#include "server.h"
#include "Config.h"

#include <iostream>

int main(int argc, char **argv)
{
	if (argc < 2) {
		std::cout << "usage:\n./registrationServer <config file>" << std::endl;
		return -1;
	}
	
	Config *config = new Config{argv[1]};
	server serv{config};
	
	return 0;
}
