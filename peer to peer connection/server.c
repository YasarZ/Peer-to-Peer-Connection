#include "./ds.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/unistd.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>


#define BUFLEN		101			/* buffer length */

// Index System Data Structures
typedef struct  {
  char nameOfContent[10]; // The name of the content [Max of 10 characters]
  char pName[10];    // Name of peer [Folder Names]
  int host;             // Port number for host
  int port;             // Port number
} contentListing;

int main(int argc, char *argv[]) 
{


  // Index System
  contentListing contentList[10]; //We want a max of 10 items in contents
  contentListing block;
  char lsList[10][10];
  
  int match = 0;
  int lsmatch = 0;
  int different = 1;
  
  int ptr = 0;
  int endptr = 0;
  
  

  // Variables used for transmitting data
  struct  pdu recvPacket,packetToSend;
  int     r_host, r_port;
  char t_port[6];
  char tcpAddress[5];

  // Variables needed for the sockets
  struct  sockaddr_in fsin;	        // Address of the client	
	char    *pts;
	int		  sock;			                // Socket of the server 
	time_t	now;			                // Time 
	int		  alen;			                // Length of the Client's address 
	struct  sockaddr_in sin;          // Enpoint address 
    int     s,sz, type;               // Socket characteristics (description and type)
	int 	  port = 3000;              // Default Port 
	int 	  counter, n,m,i,bytes_to_read,bufsize; //Variables needed for server.c
    int     errorFlag = 0;


	switch(argc){
	    case 2:
	        port = atoi(argv[1]);
	        break;
	    case 1:
	        break;
		default:
			fprintf(stderr, "Usage: %s [port]\n", argv[0]);
		exit(1);
	}
    // Creating the Socket
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;         //it's an IPv4 address
    sin.sin_addr.s_addr = INADDR_ANY; //wildcard IP address
    sin.sin_port = htons(port);       //bind to this port number
                                                                                                 
    // Creating the Socket (Allocation) 
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
		fprintf(stderr, "Socket cannot be created\n");
                                                                                
    // Binding the Sockets 
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		fprintf(stderr, "Cannot bind to %d port\n",port);
    listen(s, 5);	
	  alen = sizeof(fsin); 
    char prTemp[10], portTemp[10], cTemp[10], hTemp[10];
    while(1) {
      if (recvfrom(s, &recvPacket, BUFLEN, 0,(struct sockaddr *)&fsin, &alen) < 0)
			  fprintf(stderr, "[ERROR] Error with recvfrom\n");
      switch(recvPacket.type){
        //Here we will check for different types
        /* Requesting Content Location */
        case 'S':
          //Make the content local
          sscanf(recvPacket.data,"%s %s",prTemp,cTemp);

          match = 0;

          for(i=0;i<endptr;i++){
            if (strcmp(cTemp, contentList[i].nameOfContent) == 0){
              memset(&block,'\0', sizeof(block));
              strcpy(block.nameOfContent, contentList[i].nameOfContent);
              strcpy(block.pName, contentList[i].pName);
              block.port = contentList[i].port;
              block.host = contentList[i].host;
              // Sending host and port based on the matched address
              match=1; 
            }
            // Shift the list 1 if content is found
            if (match && i < endptr-1){
              strcpy(contentList[i].nameOfContent,contentList[i+1].nameOfContent);
              strcpy(contentList[i].pName, contentList[i+1].pName);
              contentList[i].port = contentList[i+1].port;
              contentList[i].host = contentList[i+1].host;
            }
          }

          if (match) {
            endptr = endptr - 1;
            // Commit content
            memset(&contentList[endptr],'\0', sizeof(contentList[endptr])); // Clean Struct
            strcpy(contentList[endptr].nameOfContent,block.nameOfContent);
            strcpy(contentList[endptr].pName,block.pName);
            contentList[endptr].port = block.port;
            contentList[endptr].host = block.host;
            endptr = endptr + 1;
            packetToSend.type = 'S';
            strcpy(packetToSend.data,block.pName);
			snprintf (tcpAddress, sizeof(tcpAddress), "%d", block.host);
		    snprintf (t_port, sizeof(t_port), "%d",block.port);
		    strcat(packetToSend.data, " ");
            strcat(packetToSend.data, tcpAddress);
		    strcat(packetToSend.data, " ");
            strcat(packetToSend.data, t_port);
          }
          else {
            // Error
            packetToSend.type = 'E';
					  strcpy(packetToSend.data,"The conent does not exist");
          }
          sendto(s, &packetToSend, BUFLEN, 0,(struct sockaddr *)&fsin, sizeof(fsin));
          break;
        
        /* Registration */
        case 'R':
          sscanf(recvPacket.data,"%s %s %s %s",cTemp, prTemp,hTemp,portTemp);
          r_port = atoi(portTemp);
          r_host = atoi(hTemp);
         
         errorFlag=0; 
         for(i=0;i<endptr;i++){
             // Checking to see if content was already uploaded
             if (strcmp(contentList[i].pName, prTemp)==0 && !(contentList[i].host == r_host)) {
              errorFlag = 2;
              break;
             }
             else if (strcmp(contentList[i].nameOfContent, cTemp) == 0 && strcmp(contentList[i].pName, prTemp) == 0) {
                errorFlag = 1;
             }
         }

          /*  Reply  */
          if (errorFlag==0){

            // Check if the content is new
            lsmatch=0;
            for(i=0;i<ptr;i++){
              if (strcmp(lsList[i], cTemp) == 0){
                lsmatch = lsmatch+1;
                break;
              }
            }
            // Put the content in the list
            if (!lsmatch) {
              strcpy(lsList[ptr],cTemp);
              ptr = ptr + 1;
            }

            // Put content in the list
            memset(&contentList[endptr],'\0', sizeof(contentList[endptr])); // Clean Struct
            strcpy(contentList[endptr].nameOfContent,cTemp);
            strcpy(contentList[endptr].pName,prTemp);
            contentList[endptr].port = r_port;
            contentList[endptr].host = r_host;
            endptr = endptr +=1; // Increment pointer to NEXT block
            
            // Acknowledged
            packetToSend.type = 'A';
            memset(packetToSend.data, '\0', 100);
            strcpy(packetToSend.data, prTemp);
            fprintf(stderr, "Acknowledged\n"); 
          }
          else {
            // Error
            packetToSend.type = 'E';
            memset(packetToSend.data, '\0', 100);
            if (errorFlag == 1) {
                strcpy(packetToSend.data, "Error: File Already Registered")
            }
            else if (errorFlag == 2) {
                strcpy(packetToSend.data, "Error: pName Already Registered")
            }
            fprintf(stderr,"Error\n");
          }
          sendto(s, &packetToSend, BUFLEN, 0,(struct sockaddr *)&fsin, sizeof(fsin));
          break;
        
        
        /* Deregistration */
        case 'T':
          // Make the content Local
          sscanf(recvPacket.data,"%s %s", prTemp,cTemp);
          // Remove Content
          match = 0;
          for(i=0;i<endptr;i++){
            if ((strcmp(prTemp, contentList[i].pName) == 0) && strcmp(cTemp, contentList[i].nameOfContent)==0){
              match=1;
              }
            // Shift list if content is found
            if (match && i < endptr-1){
              strcpy(contentList[i].nameOfContent,contentList[i+1].nameOfContent);
              strcpy(contentList[i].pName, contentList[i+1].pName);
              contentList[i].port = contentList[i+1].port;
              contentList[i].host = contentList[i+1].host;
            }
          }

            if (!match) {
                packetToSend.type = 'E';
                memset(packetToSend.data, '\0', 10);
                strcpy(packetToSend.data, "File Removal Error");
            }
            else {
                different = 1; 
                // Check if different, if so,
                endptr = endptr -1; 
                for(i=0;i<endptr;i++){
                    if (strcmp(cTemp, contentList[i].nameOfContent)==0){
                        different = 0;
                    }
                }
                match = 0;
                if (different){
                    for(i=0;i<ptr;i++){
                        if (strcmp(cTemp, lsList[i])==0) {
                    match=1;
                }
                if (match && i < ptr-1){
                  strcpy(lsList[i],lsList[i+1]);
                }
              }
              ptr = ptr - 1;
            }

            packetToSend.type = 'A';
            memset(packetToSend.data, '\0', 10);
            strcpy(packetToSend.data, prTemp);
            }
          sendto(s, &packetToSend, BUFLEN, 0,(struct sockaddr *)&fsin, sizeof(fsin));
          break;

        default:
          break;
        
        /* Content List */
        case 'O':
          memset(packetToSend.data, '\0', 100);
          strcpy(packetToSend.data,"");
          for(i=0;i<ptr;i++){
            strcat(packetToSend.data,lsList[i]);
            strcat(packetToSend.data," ");
          }
          packetToSend.type = 'O';
          sendto(s, &packetToSend, BUFLEN, 0,(struct sockaddr *)&fsin, sizeof(fsin));
          break;
        
      } 

    }
}