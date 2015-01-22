
/*
 Client program
 Usage: ./client hostname port filename 
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <string.h>
#include "packet.h"
#include <time.h>

int PACKET_LOSS = 0;
int PACKET_CORRUPTION = 0;

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd; //Socket descriptor
    int portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server; //contains tons of information, including the server's IP address
    

    char *filename;
    FILE* data;	//file received from server
    int expectedSeqNum = 0;
    int previousSeqNum = 0;

    srand(time(NULL));

    time_t rawtime;
    struct tm * timeinfo;

    if (argc < 4) {
       fprintf(stderr,"usage %s hostname port filename\n", argv[0]);
       exit(0);
    }
   
    filename = argv[3]; 
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); //create a new socket
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    server = gethostbyname(argv[1]); //takes a string like "www.yahoo.com", and returns a struct hostent which contains information, as IP address, address type, the length of the addresses...
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
   
   //name the socket 
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    //if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	//    error("Bind failed");

    //form initial packet containing filename request (in data field)
    struct packet requestPacket;
    strcpy(requestPacket.data, filename);
    requestPacket.dataLength = strlen(requestPacket.data);
    requestPacket.filename = 1;
    requestPacket.ack = 0;
    requestPacket.lastPacket = 0;

    //send packet to the server
    if (sendto(sockfd, &requestPacket, sizeof(struct packet), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	    error("Sendto failed");
    time (&rawtime);
    timeinfo = localtime(&rawtime);
    printf("%s ~ ", asctime(timeinfo));
    printf("Requested file %s\n", requestPacket.data);

    //open file where data will be stored
    data = fopen(strcat(filename, "_received"), "ab");	//open file in binary mode for appending

    while(1)
    {
	    int random = rand()%100;
	    struct packet receivedPacket;
	    int size = sizeof(serv_addr);
	    if(recvfrom(sockfd, &receivedPacket, sizeof(struct packet), 0, (struct sockaddr*) &serv_addr, &size) < 0 || random < PACKET_LOSS)
	    {
		    if (receivedPacket.lastPacket == 1)
			    break;
		    time (&rawtime);
    			timeinfo = localtime(&rawtime);
    			printf("%s ~ ", asctime(timeinfo));
		    printf("Data packet lost!\n");
	    }
	    else if (random < PACKET_CORRUPTION)
	    {
		    if (receivedPacket.lastPacket == 1)
			    break;
		    time (&rawtime);
    			timeinfo = localtime(&rawtime);
    			printf("%s ~ ", asctime(timeinfo));
		    printf("Received corrupt data packet!\n");
		    struct packet respondToCorruptPacket;
		    respondToCorruptPacket.seqNum = previousSeqNum;
		    respondToCorruptPacket.filename = 0;
		    respondToCorruptPacket.ack = 1;	//ack
		    respondToCorruptPacket.lastPacket = 0;
		    if(sendto(sockfd, &respondToCorruptPacket, sizeof(struct packet), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
			    error("Sendto failed");
		    time (&rawtime);
   			 timeinfo = localtime(&rawtime);
   			 printf("%s ~ ", asctime(timeinfo));
		    printf("Resend ACK for most recently received in order packet (ACK %i)\n", previousSeqNum);
	    }
	    else if (receivedPacket.seqNum != expectedSeqNum)
	    {
		   time (&rawtime);
   			 timeinfo = localtime(&rawtime);
   			 printf("%s ~ ", asctime(timeinfo));
		    printf("Received out-of-order packet (Sequence #: %i)! Discard! Expected Sequence #: %i\n", receivedPacket.seqNum, expectedSeqNum);
		   struct packet respondToUnorderedPacket;
		    respondToUnorderedPacket.seqNum = previousSeqNum;
		    respondToUnorderedPacket.filename = 0;
		    respondToUnorderedPacket.ack = 1;	//ack
		    respondToUnorderedPacket.lastPacket = 0;
		    if(sendto(sockfd, &respondToUnorderedPacket, sizeof(struct packet), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
			    error("Sendto failed");
		    time (&rawtime);
    			timeinfo = localtime(&rawtime);
    			printf("%s ~ ", asctime(timeinfo));
		    printf("Resend ACK for most recently received in order packet (ACK %i)\n", previousSeqNum); 
	    }
	    else
	    {
		if (receivedPacket.lastPacket == 1)
			  break;    
		time (&rawtime);
    		timeinfo = localtime(&rawtime);
    		printf("%s ~ ", asctime(timeinfo));
		printf("Received correct data packet with Sequence #: %i and Size: %i bytes\n", receivedPacket.seqNum, receivedPacket.dataLength);
		    
		    fwrite(receivedPacket.data, sizeof(char), receivedPacket.dataLength, data);
		  previousSeqNum += receivedPacket.dataSize;
	       	  expectedSeqNum = previousSeqNum;
		  struct packet responsePacket;
		  responsePacket.seqNum = previousSeqNum;
		  responsePacket.filename = 0;
		  responsePacket.ack = 1;	//ack
		  responsePacket.lastPacket = 0;
		  if (sendto(sockfd, &responsePacket, sizeof(struct packet), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
			  error("Sendto failed");
		  time (&rawtime);
    			timeinfo = localtime(&rawtime);
    			printf("%s ~ ", asctime(timeinfo));
		  printf("Sent ACK %i\n", previousSeqNum);
	    }
    }
    fclose(data);
	time (&rawtime);
    timeinfo = localtime(&rawtime);
    printf("%s ~ ", asctime(timeinfo));	
    printf("Entire file successfully received.\n");
    
    close(sockfd); //close socket
    
    return 0;
}
