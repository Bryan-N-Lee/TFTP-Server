
#include "tftp_server.h"
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>

using namespace std;

TFTP_SERVER* server;
int debug = 0;

void closeTFTPServer(int signum, siginfo_t* info, void* ptr){
	char ans = '0';
shutdown_prompt:
	cout << "tftp> Would you like to shutdown the Server? (y/n): ";
	cin >> ans;
	switch(ans){
		case 'n':
			return;
		case 'y':
			break;
		default:
			goto shutdown_prompt;
	}
	if(debug) cout << "main::closeTFTPServer() - Closing TFTP Server...\n";
	server->closeServer();
	if(debug) cout << "TFTP_SERVER::disconnect() - Disconnecting Clients...\n";
	for(int i = 0; i < MAX_CLIENTS; ++i)
		server->disconnect(&(server->clients[i]));
	if(debug) cout << "TFTP_SERVER::disconnect() - Closing Server...\n";
	delete server;
	kill(getpid(),SIGTERM);
}

int main(int argc, char* argv[]){
	int port = TFTP_DEFAULT_PORT;
	char* rootdir = (char*)"./";
	struct sigaction act;
	memset(&act,0,sizeof(act));
	act.sa_handler = SIG_IGN;
	act.sa_flags = SA_SIGINFO;
	act.sa_sigaction = closeTFTPServer;
	sigaction(SIGINT, &act, NULL);
	
	switch(argc){
		case 3:
			rootdir = argv[2];
			if(debug)
				cout << "TFTP Server - Main - Root Dir = " << rootdir << endl;
		case 2:
			port = atoi(argv[1]);
			if(debug)
				cout << "TFTP Server - Main - port = " << port << endl;
			break;
		default:
			if(argc > 3){
				cerr << "TFTPServer: Too Many Arguments\n";
				cout << "TFTPServer [port [rootdir]]\n";
				return 0;
			}
	}
	try{
		while(1){
			server = new TFTP_SERVER(port, rootdir, debug);
			server->run(MAX_CLIENTS);
			delete server;
		}
	}
	catch(TFTPServerException e){
		cout << "TFTPServerException Caught: " << e << endl;
		delete server;
	}
}
