struct  packet
{
	int seqNum;	//in terms of bytes (sender: first byte of packet; receiver: use for acking) 
	int dataLength;	//length of data in terms of how many elements of array used (for use in fwrite and fread)
	int dataSize;	//in terms of bytes (used for calculating seqNum on receiver part)
	int filename;	//0 = not a filename; 1 = filename
	int ack;	//0 = not an ack; 1 = ack
       int lastPacket;	//0 = not last packet; 1 = last packet
       char data[512];	//change size if needed
};       
