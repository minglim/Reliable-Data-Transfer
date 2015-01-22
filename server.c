/*
Sender Program
Usage: ./server <portnumber>
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
#include <sys/stat.h>

// default window size
#define WINDOW_SIZE 50

// an array of packets to represent the window
struct packet WINDOW[WINDOW_SIZE];

// loss/corruption
int PACKET_LOSS = 0;
int PACKET_CORRUPTION = 0;

// reporting errors 
void error(char *msg)
{
    perror(msg);
    exit(0);
}

int lowest(int x, int y)
{
	if (x < y)
		return x;
	else
		return y;
}

int main(int argc, char*argv[])
{
	// sender vars
	int portnum;
	int socketfd = 0;
	struct sockaddr_in srvr_addr;

	// receiver vars
	struct sockaddr_in clnt_addr;
	int clnt_addr_len;
	struct packet req_pack;

	// data vars 
	int n_packets;
	int n_bytes;
	FILE* data;
	struct stat s;

	// gbn vars 
	struct packet rsp_pack;
	int base;
	int nextseqnum;
	int timeout = 0; 
	int n_acks;
	struct timeval timer;
	struct timeval curr_time;
	long double trigger;
	int random;
	srand(time(NULL));

	//timestamp
	time_t rawtime;
	struct tm * timeinfo;

	// check command line arguments 
	if (argc < 2) {
       fprintf(stderr,"Usage: %s port\n", argv[0]);
       exit(0);
    }

    // get port number from arguments
    portnum = atoi(argv[1]);

    // set server address
    memset(&srvr_addr, '0', sizeof(srvr_addr));
    srvr_addr.sin_family = AF_INET;
    srvr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    srvr_addr.sin_port = htons(portnum); 

	// create socket 
	socketfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketfd < 0) 
        error("ERROR opening socket");
    if (bind(socketfd, (struct sockaddr*)&srvr_addr, sizeof(srvr_addr)) < 0)
        error("ERROR binding socket");

    // get length of client address (used to recieve request)
    clnt_addr_len = sizeof(clnt_addr);

    // run the server (infinitely)
    while(1)
    {
    	// try to recieve a request from the client
    	if (recvfrom(socketfd, &req_pack, sizeof(req_pack), 0, (struct sockaddr*) &clnt_addr, (socklen_t*) &clnt_addr_len) < 0)
            error("ERROR receiving request client");
        time (&rawtime);
	timeinfo = localtime(&rawtime);
	printf("%s ~ ", asctime(timeinfo));
	printf("Received request from %s for file %s\n", inet_ntoa(clnt_addr.sin_addr), req_pack.data);
        
        // attempt to open requested file 
        data = fopen(req_pack.data, "rb"); 
        if (data == NULL)
        	error("ERROR opening requested file"); 

        // get information about file 
        stat(req_pack.data, &s);
        n_bytes = s.st_size;
        n_packets = n_bytes / 512;	// 512 from packet.h
        if (s.st_size % 512)
        	n_packets++;				// add 1 if there is a remainder
         time (&rawtime);
	timeinfo = localtime(&rawtime);
	printf("%s ~ ", asctime(timeinfo));
	printf("Requested data divides into %d packets\n", n_packets);

        // reset window
        base = 0;
        nextseqnum = 0;
        n_acks = 0;
        bzero((char *) WINDOW, sizeof(WINDOW));

        // prepare response packets and window
        int i;
        int stop = lowest(WINDOW_SIZE, n_packets);
        bzero((char *) &rsp_pack, sizeof(rsp_pack));
        for (i = 0; i < stop; i++)
        {
        	rsp_pack.seqNum = nextseqnum;
        	rsp_pack.dataLength = fread(rsp_pack.data, 1, 512, data);
        	rsp_pack.dataSize = rsp_pack.dataLength;
        	rsp_pack.filename = 1;
        	rsp_pack.ack = 0;
        	rsp_pack.lastPacket = 0;
        	WINDOW[i] = rsp_pack;

        	// attempt to send packet back to client
        	if (sendto(socketfd, &rsp_pack, sizeof(int) * 6 + rsp_pack.dataLength, 0, (struct sockaddr *) &clnt_addr, clnt_addr_len) < 0)
                error("ERROR sending to client");
             time (&rawtime);
	timeinfo = localtime(&rawtime);
	printf("%s ~ ", asctime(timeinfo));
		printf("Packet %d sent successfully\n", rsp_pack.seqNum);

            nextseqnum += rsp_pack.dataLength;
        }
        
        // Start timer
        gettimeofday(&timer, NULL);
        
        // Timeout will be triggered at timer + 1second 
        trigger = timer.tv_sec + 0.1;
         time (&rawtime);
	timeinfo = localtime(&rawtime);
	printf("%s ~ ", asctime(timeinfo));
	printf("Expecting ACK before time: %Lf\n", trigger);

        while(n_acks < n_packets) // base <= n_packets*512
        {
            
            gettimeofday(&curr_time, NULL);
            //printf("Current time: %ld\n", curr_time.tv_sec);
            
    		// Detected timeout 
      	    if (curr_time.tv_sec > trigger)	// 0.1 seconds is timeout time
       	    {
       		     time (&rawtime);
			timeinfo = localtime(&rawtime);
			printf("%s ~ ", asctime(timeinfo));
		    printf("Timeout at packet %d\n", WINDOW[0].seqNum);
       		    // Resend all packets that have been sent but not ACKd
       		    int j;
       		    int cap = lowest(WINDOW_SIZE, n_packets-n_acks);
       		    for (j = 0; j < cap; j++)
       		    {
       			    if (WINDOW[j].seqNum > nextseqnum)
       				    break;
       			    else if (sendto(socketfd, &WINDOW[j], sizeof(int) * 6 + WINDOW[j].dataLength, 0, (struct sockaddr *) &clnt_addr, clnt_addr_len) < 0)
                        error("ERROR resending to client");
                     time (&rawtime);
			timeinfo = localtime(&rawtime);
			printf("%s ~ ", asctime(timeinfo));
			    printf("Resending packet %d in timeout mode\n", WINDOW[j].seqNum);
       		    }

                // update timer and trigger
                gettimeofday(&timer, NULL);
                trigger = timer.tv_sec + 0.1;
                 time (&rawtime);
			timeinfo = localtime(&rawtime);
			printf("%s ~ ", asctime(timeinfo));
		printf("Expecting ACK before time: %Lf\n", trigger);

       	    }
       	    
       	    struct timeval wait;
       	    wait.tv_sec = 1;
       	    wait.tv_usec = 0;
       	    fd_set readfds;
       	    FD_ZERO(&readfds);
       	    FD_SET(socketfd, &readfds);
            
        	// Check if there is something to receive 
        	if (select(socketfd+1, &readfds, NULL, NULL, &wait) > 0)
        	{
        	    // Try to receive it
            	if (recvfrom(socketfd, &req_pack, sizeof(req_pack), 0, (struct sockaddr*) &clnt_addr, (socklen_t*) &clnt_addr_len) > 0)
            	{
                    random = rand()%100;
                    if (random < PACKET_LOSS)
                    {
                         time (&rawtime);
			timeinfo = localtime(&rawtime);
			printf("%s ~ ", asctime(timeinfo));
			    printf("Data packet lost! (#%d)\n", req_pack.seqNum);
                        continue;
                    }
                    if (random < PACKET_CORRUPTION)
                    {
                         time (&rawtime);
			timeinfo = localtime(&rawtime);
			printf("%s ~ ", asctime(timeinfo));
			    printf("Received corrupt data packet! (#%d)\n", req_pack.seqNum);
                        continue;
                    }
            	
        	 	time (&rawtime);
			timeinfo = localtime(&rawtime);
			printf("%s ~ ", asctime(timeinfo));	    
		    printf("Received ACK %d\n", req_pack.seqNum);       		
        		
            		// ACK for any PACKET in WINDOW
            		while ((WINDOW[0].seqNum+WINDOW[0].dataLength) <= req_pack.seqNum && n_acks < n_packets)
            		{
            		     time (&rawtime);
				timeinfo = localtime(&rawtime);
				printf("%s ~ ", asctime(timeinfo));
				printf("Removing packet %d from window\n", WINDOW[0].seqNum);
            		    
            		    // update base
            			base = req_pack.seqNum + WINDOW[0].dataLength;
            			
            			// remove ACKd packet from window
            			int k;
            			for (k = 0; k < WINDOW_SIZE-1; k++)
            			{
            				WINDOW[k] = WINDOW[k+1];
            			}
            			
                        // update ACK count 
            			n_acks++;    		
                        
                        // add new packet to window if there are seqnums
                        if (nextseqnum < lowest(base+(WINDOW_SIZE*512), n_bytes))
                        {
                            // create response packet 
                			rsp_pack.seqNum = nextseqnum;
                			rsp_pack.dataLength = fread(rsp_pack.data, 1, 512, data);
                			rsp_pack.dataSize = rsp_pack.dataLength;
                			rsp_pack.filename = 1;
                			rsp_pack.ack = 0;
                			rsp_pack.lastPacket = 0;
                			
                			// add to window
                			WINDOW[k] = rsp_pack;
                			
                			//nextseqnum += rsp_pack.dataLength;
                			
                			// attempt to send
            			    if (sendto(socketfd, &rsp_pack, sizeof(int) * 6 + rsp_pack.dataLength, 0, (struct sockaddr *) &clnt_addr, clnt_addr_len) < 0)
                                error("ERROR on sending");
                            	 time (&rawtime);
				timeinfo = localtime(&rawtime);
				printf("%s ~ ", asctime(timeinfo));
				    printf("Sent new packet number %d after receiving an ACK\n", nextseqnum);
nextseqnum += rsp_pack.dataLength;
                            
                            // Update timer and trigger 
                			gettimeofday(&timer, NULL);	
                            trigger = timer.tv_sec + 0.1;
                             time (&rawtime);
				timeinfo = localtime(&rawtime);
				printf("%s ~ ", asctime(timeinfo));
			    printf("Expecting ACK before time: %Lf\n", trigger);
                        }                       

            		}

            	}
            	            	
           }

        }

        // Send teardown ACK (last packet)
        bzero((char *) &rsp_pack, sizeof(rsp_pack));
        rsp_pack.lastPacket = 1;
        rsp_pack.seqNum = nextseqnum;
         time (&rawtime);
	timeinfo = localtime(&rawtime);
	printf("%s ~ ", asctime(timeinfo));
	printf("Teardown\n");
        if (sendto(socketfd, &rsp_pack, sizeof(int) * 6, 0, (struct sockaddr *) &clnt_addr, clnt_addr_len) < 0)
            error("ERROR sending final packet");
        fclose(data);

    }
}
