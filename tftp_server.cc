
#include "tftp_server.h"

/*
 *	Sets up TFTP Server
 *
 *	@param	port	Port Number
 *	@param	dir		Server's (Root) Directory Location
 *	@param	db		Debugging (0 = no | !0 = yes)
 *	@action			Server is established and ready to accept clients
 */

TFTP_SERVER::TFTP_SERVER(int _port, char* _dir, int _db = 0){
	DEBUG = _db;
	server_port = _port;
	strcpy(rootdir,_dir);
	
	if((server_socketfd = socket(AF_INET, SOCK_DGRAM,0)) < 0){
		if(DEBUG) cerr << "[Error] TFTP_SERVER::TFTP_SERVER() - socket()\n";
		throw TFTPServerException((char*)"Socket Error");
	}
	
	if(DEBUG) cout << "TFTP_SERVER::TFTP_SERVER() - socket() is OK...\n";
	server_addr.sin_family = AF_UNSPEC;			// host byte order
	server_addr.sin_port = htons(server_port);	// short, network byte order
	server_addr.sin_addr.s_addr = INADDR_ANY;	// auto-fill with my IP
	memset(&(server_addr.sin_zero),0,8);		// zero the rest of the struct
	
	if(bind(server_socketfd,(struct sockaddr*)&server_addr,sizeof(struct sockaddr)) < 0){
		if(DEBUG) cerr << "[Error] TFTP_SERVER::TFTP_SERVER() - bind()\n";
		close(server_socketfd);
		throw TFTPServerException((char*)"Bind Error"); }
	
	if(DEBUG) cout << "TFTP_SERVER::TFTP_SERVER() - bind() is OK...\n";
}

/*
 *
 *
 */
int TFTP_SERVER::run(int max_clients){
	if(DEBUG) cout << "TFTP_SERVER::run() - TFTP Server is running...\n";
	while(true){
		int n = receivePacket(&(clients[0]));
		if(n == -1){
			cerr << "[Error] TFTP_Server::run() - Poll returned with error\n";
			closeServer();
			return 0;
		}
		else if(n < -1){
			cerr << "[Error] TFTP_Server::run() - recvfrom returned with error\n";
			return 0;
		}
		if(n == 0) continue;
		if(processClient(&(clients[0])) == 0){
			if(DEBUG) cout << "TFTP_SERVER::run() - Disconnecting Client: "
							<< clients[0].ip << endl;
			disconnect(&clients[0]);
		}
	}
}

/*
 *	Receive packet from one client
 *
 *	@param	client		A client
 *	@return				-1 - Error | 0 - Timeout | >0 - # of bytes received
 */
int TFTP_SERVER::receivePacket(Client* client){
	int bytes_recv = 0;
	struct pollfd ufd;
	ufd.fd = server_socketfd;
	ufd.events = POLLIN;
	int rv = poll(&ufd,1,4000); // 4 second timeout
	if(rv < 0){
		if(DEBUG) cerr << "[Error] TFTP_SERVER::receivePacket() - poll()\n";
		return -1;
		/* Throw Exception */ }
	else if(rv == 0){
		if(DEBUG) cout << "TFTP_SERVER::receivePacket() - Poll Timeout...\n";
		return 0;
		/* Timeout */ }
	else{// Or client->client_socket
		client->receive_packet.clearPacket();
		client->send_packet.clearPacket();
		socklen_t len = sizeof(client->address);
		//char buffer[512];
		bytes_recv = recvfrom(server_socketfd,					//Socket fd
							  client->receive_packet.getData(0),//buffer
							  //&buffer,
							  //512,
							  TFTP_PACKET_MAX_SIZE,				//Size of buffer
							  0,
							  (struct sockaddr*)&(client->address),
							  &len);
		client->receive_packet.setSize(bytes_recv);
		if(bytes_recv < 0){
			if(DEBUG)
				cout << "TFTP_SERVER::receivePacket() - recvfrom error: " << errno << endl;
			return -2;
		}
		if(DEBUG){
			cout << "TFTP_SERVER::receivePacket() - Packet Received ("
				<< bytes_recv << " Bytes) from "
				<< inet_ntoa(client->address.sin_addr) << "...\n";
			cout << "TFTP_SERVER::receivePacket() - Packet Type: \""
				<< (int)*(client->receive_packet.getData(1)) << "\"...\n";
		}
		return bytes_recv;
	}
	return 0;
}

/*
 *	Received Packet, process it accordingly
 *	@param	client		The Client
 *	@return				0		-> If disconnecting after sending ACK|DATA
 *						else	-> Packet Type received
 */
int TFTP_SERVER::processClient(Client* client){
	client->ip = (char *)inet_ntoa(client->address.sin_addr);
	if(DEBUG) cout << "TFTP_SERVER::processClient() - Client IP: "
				<< client->ip << endl;
	switch(client->receive_packet.getOpcode()){
		case TFTP_OPCODE_RRQ:{
			/* Find the read file and create a Read Packet to send back */
			if(DEBUG) cout << "TFTP_SERVER::processClient() - RRQ Received from "
							<< client->ip << "...\n";
			client->request_type = REQUEST_READ;
			if((client->client_socket = socket(AF_INET, SOCK_DGRAM,0)) < 0){
				if(DEBUG) cerr << "[Error] TFTP_SERVER::processClient() - RRQ socket()\n";
				return -1; // Throw Exception
			}
			/* Determine if a dir request or file request */
			char RRQ_filename[MAX_PATH_LENGTH];
			if(client->receive_packet.getString(2,RRQ_filename,MAX_PATH_LENGTH) == 0){
				if(DEBUG)
					cout << "[Error] TFTP_SERVER::processClient()-TFTP_PACKET::getString() - returned 0\n";
					return 0;
			}
			cout << "RRQ_FILENAME[0] = " << RRQ_filename[0] << endl;
			if(RRQ_filename[0] == '?'){
				if(strlen(RRQ_filename) > 1){
					if(createDirPacket(client,&(RRQ_filename[1])) < 0){
						cout << "TFTP_SERVER::processClient() - Error finding Directory\n";
						return 0;
					}
				}
				else
					if(createDirPacket(client,(char*)".") < 0){
						if(DEBUG) cout << "TFTP_SERVER::processClient() - Error Openning Directory";
						return 0;
					}
			} else{
				if(getReadFile(client) < 0){
					if(DEBUG) cerr << "[Error] TFTP_SERVER::processClient() - Error Getting Read File\n";
					return 0;
				}
				createReadPacket(client);
			}
			if(sendPacket(&(client->send_packet), client) < 0){
				if(DEBUG) cout << "TFTP_SERVER::sendPacket() - RRQ - sendto returned error ("
					<< errno << ")\n";
			}
			
			if(client->disconnect_after_send)
			{ disconnect(client); return 0; }
			
			return TFTP_OPCODE_RRQ;
		}
		case TFTP_OPCODE_WRQ:{
			/* Create the write file and an ACK Packet to send back */
			if(DEBUG) cout << "TFTP_SERVER::processClient() - WRQ Received from "
							<< client->ip << "...\n";
			client->request_type = REQUEST_WRITE;
			if((client->client_socket = socket(AF_INET, SOCK_DGRAM,0)) < 0){
				if(DEBUG) cerr << "[Error] TFTP_SERVER::processClient() - WRQ socket()\n";
				return 0; // Throw Exception
			}
			createWriteFile(client); // << CHANGE THIS LATER
			
			/* Send ACK Back */
			TFTP_PACKET* ack_packet_WRQ = new TFTP_PACKET();
			ack_packet_WRQ->createACK(client->block);
			if(sendPacket(ack_packet_WRQ, client) < 0){
				if(DEBUG) cout << "TFTP_SERVER::sendPacket() - WRQ - sendto returned error ("
					<< errno << ")\n";
			}
			delete ack_packet_WRQ;
			/* ~~~~~~~~~~~~~ */
			
			return TFTP_OPCODE_WRQ;
		}
		case TFTP_OPCODE_DATA:{
			/* Write Packet data to file */
			if(DEBUG) cout << "TFTP_SERVER::processClient() - DATA Received from "
							<< client->ip << "...\n";
			writeData(client);
			
			/* Send ACK Back */
			TFTP_PACKET* ack_packet_DATA = new TFTP_PACKET();
			ack_packet_DATA->createACK(client->block);
			if(sendPacket(ack_packet_DATA, client) < 0){
				if(DEBUG) cout << "TFTP_SERVER::sendPacket() - DATA - sendto returned error ("
					<< errno << ")\n";
			}
			delete ack_packet_DATA;
			/* ~~~~~~~~~~~~~ */
			
			if(client->disconnect_after_send){
				cout << "TFTP::processClient() - DATA - Disconnecting...\n" << endl;
				/*disconnect(client);*/ return 0; }
			
			return TFTP_OPCODE_DATA;
		}
		case TFTP_OPCODE_ACK:{
			/* Prepare to send next Read Packet */
			if(DEBUG) cout << "TFTP_SERVER::processClient() - ACK Received from "
							<< client->ip << "...\n";
			createReadPacket(client);
			if(sendPacket(&(client->send_packet),client) < 0){
				if(DEBUG) cout << "TFTP_SERVER::sendPacket() - ACK - sendto returned error ("
								<< errno << ")\n";
			}
			
			if(client->disconnect_after_send){
				cout << "TFTP::processClient() - ACK - Disconnecting...\n" << endl;
				/*disconnect(client);*/ return 0; }
			
			return TFTP_OPCODE_ACK;
		}
		case TFTP_OPCODE_ERROR:{
			/* Something went wrong, quit */
			if(DEBUG) cout << "TFTP_SERVER::processClient() - ERROR Received from "
							<< client->ip << "...\n";
			return TFTP_OPCODE_ERROR;
		}
		default:{
			/* Unknown Packet received, send back Error packet */
			return -1;
		}
	}
	return -1;
}

/*
 *	Find the File to be read and set it in the client object
 *
 */
int TFTP_SERVER::getReadFile(Client* client){
	if(DEBUG) cout << "TFTP_SERVER::getReadFile() - " << client->ip
					<< " - Finding Read File...\n";
	char* filename = new char[TFTP_PACKET_MAX_SIZE];
	char actual_file[TFTP_PACKET_MAX_SIZE];
	
	strcpy(filename,rootdir);
	
	client->receive_packet.getString(2,(filename + strlen(filename)),
									client->receive_packet.getSize());
	if(DEBUG) cout << "TFTP_SERVER::getReadFile() - Getting: " << filename << endl;
	char at[] = "@";
	strncpy(actual_file,filename,strcspn(filename,at)+1);
	
	if(DEBUG) cout << "TFTP_SERVER::getReadFile() - Actual File: " << actual_file << endl;
	client->read_file = new ifstream(filename,ios::binary | ios::in | ios::ate);
	
	if(!client->read_file->is_open() || !client->read_file->good()){
		if(DEBUG){
			cout << "TFTP_SERVER::getReadFile() - Could not open file: "
					<< actual_file << endl;
			cout << "TFPT_SERVER::getReadFile() - Sending Error Packet\n";
		}
		sendError(client,ERROR_FILE_NOT_FOUND,(char*)"File Not Found");
		disconnect(client);
		return -1;
	}
	client->read_file->seekg(getFileOffset(filename),ios::beg);
	
	if(DEBUG) cout << "TFTP_SERVER::getReadFile() - File Openned: " << actual_file << endl;
	
	delete[] filename;
	return 0;
}

/*
 *	Create the File to be written to and set it in the client object
 *
 */
int TFTP_SERVER::createWriteFile(Client* client){
	if(DEBUG) cout << "TFTP_SERVER::createWriteFile() - " << client->ip
					<< " - Creating Write File...\n";
	char* filename = new char[TFTP_PACKET_MAX_SIZE];
	char actual_file[TFTP_PACKET_MAX_SIZE];
	
	strcpy(filename,rootdir);
	
	client->receive_packet.getString(2,(filename + strlen(filename)),
									 client->receive_packet.getSize());
	char at[] = "@";
	strncpy(actual_file,filename,strcspn(filename,at));
	
	if(DEBUG) cout << "TFTP_SERVER::createWriteFile() - File (" << actual_file << ") created...\n";
	
	client->write_file = new ofstream(actual_file, ios::binary);
	
	client->write_file->seekp(getFileOffset(filename),ios::beg);
	return 0;
}

/*
 *	Read data from file and create packet
 *
 */
int TFTP_SERVER::createReadPacket(Client* client){
	if(DEBUG) cout << "TFTP_SERVER::createReadPacket() - " << client->ip
					<< " - Creating Read Packet...\n";
	char _data[TFTP_PACKET_DATA_SIZE];
	client->read_file->read(_data,TFTP_PACKET_DATA_SIZE);
	
	if(client->read_file->eof()){
		if(DEBUG) cout << "TFTP_SERVER::creatReadPacket() - End of File Reached\n" << endl;
		client->disconnect_after_send = true;
	}
	client->send_packet.createData(++client->block,(char*)_data,
								   client->read_file->gcount());
	if(DEBUG){
		cout << "TFTP_SERVER::createReadPacket() - " << client->ip
			<< ": Packet (" << client->block - 1 << ") sent...\n";
		client->send_packet.printData();
	}
	return 0;
}

/*
 *	Write the data of the packet to the file
 *
 *	@param	client		Current Client
 *	@return				1 = Last Packet | 0 = Middle Packet | -1 = Out of Order Packet
 */
int TFTP_SERVER::writeData(Client* client){
	if(DEBUG) cout << "TFTP_SERVER::writeData() - " << client->ip
					<< " - Writing Data...\n";
	if(++client->block == client->receive_packet.getBlockNumber()){
		if(DEBUG) cout << "TFTP_SERVER::writeData() - Block (" << client->block << ") Received...\n";
		
		char _data[TFTP_PACKET_DATA_SIZE];

		int bytes_written = (client->receive_packet.getSize() - 4);

		client->receive_packet.copyData(4,_data,bytes_written);
		
		client->write_file->write(_data,bytes_written);
		
		if(DEBUG) cout << "TFTP_SERVER::writeData() - " << bytes_written << " Bytes written\n";
		
		if(client->receive_packet.getSize() < TFTP_PACKET_DATA_SIZE + 4){
			client->write_file->close();
			client->disconnect_after_send = true;
			//disconnect(client);
			return bytes_written;
		}
		return bytes_written;
	}
	return -1;
}

/*
 *	Create a Read like Packet with the contents of the requested Directory
 *
 *
 */
int TFTP_SERVER::createDirPacket(Client* client, char* dir){
	if(DEBUG){
		cout << "TFTP_SERVER::createDirPacket() - Creating Directory Packet"
					<< " for " << client->ip << " for Directory "
					<< dir << endl;
	}
	int _data_size = 0;
	if(client->dirPost == 0){
		if(ls(dir,client->dirBuf) < 0){
			if(DEBUG){
				cout << "TFTP_SERVER::createDirPacket() - Could not open Directory: "
				<< dir << endl;
				cout << "TFPT_SERVER::createDirPacket() - Sending Error Packet\n";
			}
			sendError(client,ERROR_FILE_NOT_FOUND,(char*)"Directory Not Found");
			disconnect(client);
			return -1;
		}
	}
	(strlen(&(client->dirBuf[client->dirPost])) > 512) ?
		_data_size = 512 :
		_data_size = strlen(&(client->dirBuf[client->dirPost]));
	client->send_packet.createData(++client->block,&(client->dirBuf[client->dirPost])
									,_data_size);
	client->dirPost += _data_size;
	if(strlen(&(client->dirBuf[client->dirPost])) <= 512)
		client->disconnect_after_send = 1;
	
	if(DEBUG){
		cout << "TFTP_SERVER::createDirPacket() - " << client->ip
		<< ": Packet (" << client->block - 1 << ") sent...\n";
		client->send_packet.printData();
	}
	return 0;
	
}

/*
 *	Send client data from file (for RRQs)
 *
 */
int TFTP_SERVER::sendPacket(TFTP_PACKET* _packet, Client* client){
	/*if(client->connection == NOT_CONNECTED){
		if(DEBUG)
			cout << "TFTP_SERVER::sendPacket() - Attempted to Send to not connected client" << endl;
		return -1;
	}*/
	if(DEBUG) cout << "TFTP_SERVER::sendPacket() - Sending Packet (" << "\""
					<< *_packet << "\") to " << client->ip << "...\n";
	socklen_t len = sizeof(client->address);
	int n = sendto(client->client_socket, _packet->getData(0),
				   _packet->getSize(), 0,
				   (struct sockaddr*)&(client->address), len);
	if(DEBUG) cout << "TFTP_SERVER::sendPacket() - Packet Sent ("
					<< _packet->getSize() << " Bytes)...\n";
	return n;
}


int TFTP_SERVER::sendError(Client* client, int error_code, char* msg){
	if(DEBUG) cout << "TFTP_SERVER::sendError() - Sending Error to \""
					<< client->ip << "\"...\n";
	TFTP_PACKET* error_packet = new TFTP_PACKET();
	error_packet->createError(error_code, msg);
	sendPacket(error_packet, client);
	delete error_packet;
	return 0;
}

int TFTP_SERVER::disconnect(Client* client){
	//if(DEBUG) cout << "TFTP_SERVER::disconnect() - Disconnecting Client (" << client->ip << ")...\n";
	if(!client) return 0;
	client->receive_packet.clearPacket();
	client->send_packet.clearPacket();
	//strcpy(client->ip,(char*)"");
	client->ip = (char*)"";
	memset(client->dirBuf,0,DIRECTORY_LIST_SIZE);
	client->connection = NOT_CONNECTED;
	client->dirPost = 0;
	client->block = 0;
	client->request_type = REQUEST_UNDEFINED;
	client->temp = 0;
	client->disconnect_after_send = false;
	if(client->client_socket > 0) close(client->client_socket);
	//if(client->read_file) delete client->read_file;
	//if(client->write_file) delete client->write_file;
	//delete ifstream
	//delete ofstream
	return 0;
}

int TFTP_SERVER::closeServer(){
	if(DEBUG) cout << "TFTP_SERVER::closeServer() - Closing TFTP Server\n";
	if(server_socketfd > 0) close(server_socketfd);
	return 0;
}

int TFTP_SERVER::ls(char* dirName, char* b){
	int slash_at_end = 0;
	if(dirName[strlen(dirName)-1] == '/')
		slash_at_end = 1;
	
	DIR* dirp;
	if(!(dirp = opendir(dirName)))	//I don't know how important this is, but
	{ 								// if test is a dir and test3 is another dir in
		cout << "My_LS::openDir() - Couldn't Open Directory: "
		<< dirName << endl;
		return -1;
	}
	DIR* file;
	struct dirent* direntp, *result;
	struct dirent entry;
	string dirlist = "";
	string currentSpot = dirName;		//beginning directory
	string temp = currentSpot;
	if(!slash_at_end) temp += "/";	//path to a directory
	currentSpot = temp;					//
	char* name;							//
	struct stat buf;
	int return_code = 0;
	for (return_code = readdir_r(dirp, &entry, &result);
		 result != NULL && return_code == 0;
		 return_code = readdir_r(dirp, &entry, &result)){
		if(entry.d_name[0] == '.') continue;
		temp += entry.d_name;		//adds the name of the dir/file to the path
		name = strdup(temp.c_str());	//converts temp to char*
		if(!(file = opendir(name))){		//checking if a dir or file
			dirlist += entry.d_name;
			stat(name,&buf);
			stringstream ss;
			ss << buf.st_size;
			dirlist += ("|" + ss.str() + "\n");
		}
		else{
			dirlist += entry.d_name;
			dirlist += "/\n";
		}
		temp = currentSpot;				//reset the path
	}
	strcpy(b,dirlist.c_str());
	return 0;
}

TFTP_SERVER::~TFTP_SERVER(){}
