
#include "tftp_packet.h"

using namespace std;

/*
 *	Constructor
 */
TFTP_PACKET::TFTP_PACKET(){
	clearPacket();
}

/*	
 *	Returns packet's current size
 *	
 *	@return	Packet's Size
 */
int TFTP_PACKET::getSize()
{ return packet_size; }

/*	
 *	Sets packet's current size
 *
 *	@param	size	new Packet size
 *	@return			Packet's new size
 */
int TFTP_PACKET::setSize(int _size)
{ return _size <= TFTP_PACKET_MAX_SIZE ? (packet_size = _size) : packet_size; }

/*
 *	Clear the packet's contents
 *
 *	@action			Set Packet to 0
 */
void TFTP_PACKET::clearPacket()
{ memset(data, packet_size = 0, TFTP_PACKET_MAX_SIZE); }

/*
 *	Prints Packet's Contents
 *
 *	@action			Prints the data member variable
 */
void TFTP_PACKET::printData(){
	cout << "\n~~~~~~~~~~~~~~~Data~~~~~~~~~~~~~~~\n";
	cout << "\tPacket Size: " << packet_size << endl;
	cout << "Opcode: " << (int)data[0] << (int)data[1] << endl;
	switch(getOpcode()){
		case 3:
			cout << "Block #: " << (int)data[2] << (int)data[3] << endl;
			for(int i = 4; i < packet_size; ++i)
				cout << data[i];
			break;
		case 4:
			cout << "Block #: " << (int)data[2] << (int)data[3] << endl;
			break;
		case 5:
			cout << "Error Code: " << (int)data[2] << (int)data[3] << endl;
			int n = strlen((char*)getData(4));
			for(int i = 4; i < n; ++i)
				cout << (char)data[i];
			break;
	}
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
}

/*
 *	Add Byte to end of Packet's contents
 *
 *	@param	b		Byte to be written at packet_size + 1
 *	@return			Byte written | -1 if reached max size
 */
int TFTP_PACKET::addByte(BYTE _b){
	if(packet_size >= TFTP_PACKET_MAX_SIZE){
		cerr << "Max Packet Size Reached (" << packet_size << ")\n";
		return -1;
	}
	return (data[packet_size++] = (unsigned char)_b);
}


/*
 *	Add a WORD (uint16_t) to 2 Bytes of data
 *
 *	@param	w		WORD to be added
 *	@return			The WORD that was added || -1 if reached max size
 */
int TFTP_PACKET::addWord(WORD _w){
	if(packet_size + 2 >= TFTP_PACKET_MAX_SIZE){
		cerr << "Max Packet Size Reached (" << packet_size << ")\n";
		return -1;
	}
	data[packet_size++] |= (_w>>8);
	data[packet_size++] |= _w;
	return _w;
}

/*
 *	Add a string to the Packet's contents
 *
 *	@param	s		String to be appended to data
 *	@return			Number of Bytes appended to data
 */
int TFTP_PACKET::addString(char* _s){
	int i = 0;// = strlen(_s);
	for(; _s[i]; ++i)
		if(addByte(_s[i]) < 0) return i;
	packet_size += i;
	return i;
}
int TFTP_PACKET::addString(const char* _s){
	int i = 0;
	for(; _s[i]; ++i)
		if(addByte(_s[i]) < 0) return i;
	packet_size += i;
	return i;
}

/*
 *	Appends the string to the packet
 *
 *	@param	buf		String buffer
 *	@param	len		Buffer's length
 *	@return			Number of bytes (chars) written
 */
int TFTP_PACKET::addData(char* _buf, int _len){
	if(packet_size + _len >= TFTP_PACKET_MAX_SIZE){
		cerr << "Packet Max Size Reached (" << packet_size + _len << ")\n";
		return 0;
	}
	memcpy(&(data[packet_size]),_buf,_len);
	packet_size += _len;
	return _len;
}

/*
 *	Returns the data byte at the given index/offset
 *	
 *	@param	offset	Offset/index of Byte/Data to be returned
 *	@return			Byte at offset
 */
BYTE TFTP_PACKET::getByte(int _offset)
{ return (BYTE)data[_offset]; }

/*
 *	Returns a Word in the Packet's content
 *
 *	@param offset	Offset in data
 *	@return			Word starting at offset
 */
WORD TFTP_PACKET::getWord(int _offset)
{ return ((getByte(_offset)<<8)|getByte(_offset+1)); }

/*
 *	Returns the Opcode of the Packet
 *
 *	@return			The Packet's Opcode
 */
BYTE TFTP_PACKET::getOpcode()
{ return (BYTE)data[1]; }

/*
 *	Copies from the Data buffer to given buffer
 *
 *	@param	offset	Data to be copied starts
 *	@param	buf		Destination Buffer
 *	@param	len		Length of Copy
 *	@return			Number of Bytes copied
 */
int TFTP_PACKET::getString(int _offset, char* _buf, int _len){
	if(_len < packet_size - _offset || _offset >= packet_size) return 0;
	strcpy(_buf,(char*)&(data[_offset]));
	//memcpy(_buf, &(data[_offset]), packet_size - _offset);
	return packet_size - _offset;
	
}

/*
 *	Returns the Block/Sequence Number of this Packet
 *
 *	@return			Block/Sequence of Packet (0 for Error & RRQ/WRQ packet)
 */
WORD TFTP_PACKET::getBlockNumber()
{ return (this->isData() || this->isACK()) ? this->getWord(2) : 0; }

/*
 *	Returns an address at the given offset
 *
 *	@param	offset	Offset from packet contents
 *	@return			Address to Data at offset
 */
unsigned char* TFTP_PACKET::getData(int _offset = 0)
{ return &(data[_offset]); }

/*
 *	Returns the Size of the Data in the packet
 *
 *	@param	offset	Offset from packet contents
 *	@return			Length of Data field
 */
int TFTP_PACKET::getDataSize(){
	if(!isData()) return 0;
	BYTE* d = getData(TFTP_DATA_PKT_DATA_OFFSET);
	int n = 0;
	for(;d[n] && n < TFTP_PACKET_DATA_SIZE;++n);
	return n;
}

/*
 *	Copies data buffer to destination buffer
 *	
 *	@param	offset	Offset from data buffer
 *	@param	dest	Destination string
 *	@param	len		Number of bytes to be copied
 *	@return			Number of bytes copied
 */
int TFTP_PACKET::copyData(int _offset, char* _dest, int _len){
	if(_len < packet_size - _offset || _offset >= packet_size) return 0;
	memcpy(_dest, &(data[_offset]), packet_size - _offset);
	return packet_size - _offset;
}


/*
 *	Create RRQ Packet
 *
 *	2 bytes     string    1 byte   string    1 byte
 *	------------------------------------------------
 *	| Opcode |  Filename  |  0  |   Mode    |   0  |
 *	------------------------------------------------
 *
 *	@param	filename	Filename of read request
 *	@return				Number of Bytes of the filename was written || -1 if error
 */
int TFTP_PACKET::createRRQ(char* _filename){
	clearPacket();
	if(addWord(TFTP_OPCODE_RRQ) < 0) return -1;
	int n = addString(_filename);
	if(addByte(0) < 0) return -1;
	if(addString(TFTP_DEFAULT_TRANSFER_MODE) < strlen(TFTP_DEFAULT_TRANSFER_MODE))
		return -1;
	if(addByte(0) < 0) return -1;
	return n;
}

/*
 *	Create WRQ Packet
 *
 *	2 bytes     string    1 byte   string    1 byte
 *	------------------------------------------------
 *	| Opcode |  Filename  |  0  |   Mode    |   0  |
 *	------------------------------------------------
 *
 *	@param	filename	Filename of write request
 *	@return				Number of Bytes of the filename was written || -1 if error
 */
int TFTP_PACKET::createWRQ(char* _filename){
	clearPacket();
	if(addWord(TFTP_OPCODE_WRQ) < 0) return -1;
	int n = 0;
	if((n = addString(_filename)) < strlen(_filename)){
		cerr << "Not all of Filename was written (" << n << " / ";
		cerr << strlen(_filename) << ")\n";
		return -1;
	}
	if(addByte(0) < 0) return -1;
	if(addString(TFTP_DEFAULT_TRANSFER_MODE) < strlen(TFTP_DEFAULT_TRANSFER_MODE))
	   return -1;
	if(addByte(0) < 0) return -1;
	return n;
}

/*
 *	Create ACK Packet
 *
 *	2 bytes    2 bytes
 *	---------------------
 *	| Opcode |  Block # |
 *	---------------------
 *
 *	@param	packet_num	ACK Packet Number (sequence)
 *	@return				ACK Packet Number (sequence) || -1 if error
 */
int TFTP_PACKET::createACK(int _packet_num){
	clearPacket();
	if(addWord(TFTP_OPCODE_ACK) < 0) return -1;
	if(addWord(_packet_num) < 0)	return -1;
	return _packet_num;
}
/*
 *	Create DATA Packet
 *
 *	2 bytes    2 bytes     n bytes
 *	---------------------------------
 *	| Opcode |  Block # |   Data    |
 *	---------------------------------
 *
 *	@param	block		Block Number / Sequence Number
 *	@param	data		Data buffer
 *	@param	data_size	Size of buffer
 *	@return				Number Bytes of Data written || -1 if error
 */
int TFTP_PACKET::createData(int _block, char* _data, int _data_size){
	clearPacket();
	if(addWord(TFTP_OPCODE_DATA) < 0) return -1;
	if(addWord(_block) < 0) return -1;
	return addData(_data,_data_size);
}

/*
 *	Create ERROR Packet
 *
 *	2 bytes    2 bytes     string    1 byte
 *	-----------------------------------------
 *	| Opcode | ErrorCode |  ErrMsg  |   0   |
 *	-----------------------------------------
 *
 *	@param	error_code	The Error Code
 *	@param	msg			The Error Message
 *	@return				The Error Code || -1 if error
 */
int TFTP_PACKET::createError(int _error_code, char* _msg){
	clearPacket();
	if(addWord(TFTP_OPCODE_ERROR) < 0) return -1;
	if(addWord(_error_code) < 0) return -1;
	if(addString(_msg) < strlen(_msg)) return -1;
	if(addByte(0) < 0) return -1;
	return _error_code;
}

/*
 *	Returns if Packet is a Read Request Packet
 */
bool TFTP_PACKET::isRRQ()
{ return getOpcode() == TFTP_OPCODE_RRQ; }

/*
 *	Returns if Packet is a Write Request Packet
 */
bool TFTP_PACKET::isWRQ()
{ return getOpcode() == TFTP_OPCODE_WRQ; }

/*
 *	Returns if Packet is a Acknowledgment Packet
 */
bool TFTP_PACKET::isACK()
{ return getOpcode() == TFTP_OPCODE_ACK; }

/*
 *	Returns if Packet is a Data Packet
 */
bool TFTP_PACKET::isData()
{ return getOpcode() == TFTP_OPCODE_DATA; }

/*
 *	Returns if Packet is a Error Packet
 */
bool TFTP_PACKET::isError()
{ return getOpcode() == TFTP_OPCODE_ERROR; }

/*
 *	Destructor
 */
TFTP_PACKET::~TFTP_PACKET(){}
