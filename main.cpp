#include "server.h"
#include "Config.h"

#include <iostream>
#include <csignal>

server *serv;
Config *config;


void signalHandler( int signum ) {
   std::cerr << "Interrupt signal (" << signum << ") received.\n";
   
   switch (signum)
   {
//		case SIGPIPE :
//			fprintf(stderr, "\n\nBROKEN PIPE\n\n");
//			return;
		case SIGINT :
		case SIGHUP :
		case SIGTERM :
		case SIGTSTP :
			fprintf(stderr, "graceful shutdown...\n");
			serv->shutdown(false);
			break;
		case SIGKILL :
			fprintf(stderr, "hard shutdown....\n");
			serv->shutdown(true);
			break;
		default :
			fprintf(stderr, "No handler for signal: %d\n", signum);
   }


   exit(signum);  
}


int main(int argc, char **argv)
{
	if (argc < 2) {
		std::cout << "usage:\n./registrationServer <config file>" << std::endl;
		return -1;
	}
	
	// ignore broken pipes on writes instead of exiting
	signal(SIGPIPE, SIG_IGN);
	
	// register my handlers
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
