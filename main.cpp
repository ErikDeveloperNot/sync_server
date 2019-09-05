#include "server.h"
#include "Config.h"

#include <iostream>
#include <csignal>

server *serv;
Config *config;


void signalHandler( int signum ) {
   std::cout << "Interrupt signal (" << signum << ") received.\n";
   
   switch (signum)
   {
		case SIGINT :
		case SIGHUP :
		case SIGTERM :
		case SIGTSTP :
			printf("graceful shutdown...\n");
			serv->shutdown(false);
			break;
		case SIGKILL :
			printf("hard shutdown....\n");
			serv->shutdown(true);
			break;
		default :
			printf("No handler for signal: %d\n", signum);
   }


   exit(signum);  
}


int main(int argc, char **argv)
{
	if (argc < 2) {
		std::cout << "usage:\n./registrationServer <config file>" << std::endl;
		return -1;
	}
	
	signal(SIGINT, signalHandler);  
	signal(SIGHUP, signalHandler);  
	signal(SIGKILL, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGTSTP, signalHandler);
	
	config = new Config{argv[1]};
	serv = new server{config};
	serv->start();
	
	return 0;
}
