#include <sys/types.h>
#include <sys/socket.h> 
#include <sys/time.h> 
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>                                                                         
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// User defined files
#include "ds.h"

#define	BUFLEN      3000     // Max bytes per packet

// Globals
char pName[10];                             // Contains the user name, same name per instance of client
char localContent[5][10];                   // Creates an array of strings that represent the name of the local content that has been registered
                                            // via the index server. Can store total of 5 elements of 10 bytes per content (9 + null terminating char)   
char localPort[5][6];                       // Stores the port number associated with each piece of locally registered content
int localContentNum = 0;                    // Stores the number of saved local contents 

// TCP Socket variables
int tcpArr[5] = {0};                        // 5 TCP ports to listen to
struct sockaddr_in socArr[5];              
int max_sd = 0;
int activity;                               // Tracks the I/O activity on sockets
fd_set  readfds;                            // List of socket descriptors

// UDP connection variables
char	*host = "localhost";                // Default UDP host
int		port = 3000;                        // Default UDP port                
int		s, t, newT, n, type;	    // Initializing the socket descriptor and socket type	
struct 	hostent	*ptr;	                    // A pointer to host information entry	

void addToLocalContent(char nameOfContent[], char port[], int socket, struct sockaddr_in server){ //CHANGED contentName to nameOfContent
    strcpy(localContent[localContentNum], nameOfContent);
    strcpy(localPort[localContentNum], port);
    tcpArr[localContentNum] = socket;
    socArr[localContentNum] = server;
    if (socket > max_sd){
        max_sd = socket;
    }
    localContentNum++;
}

void removeFromLocalContent(char nameOfContent[]){
    int j;
    int found = 0;  

    // This for loop goes through the local list of content names for the requested content name
    for(j = 0; j < localContentNum; j++){
        if(strcmp(localContent[j], nameOfContent) == 0){
             // Sets terminating characters to all elements
            memset(localContent[j], '\0', sizeof(localContent[j]));         
            memset(localPort[j], '\0', sizeof(localPort[j]));  
            tcpArr[j] = 0;    

            // The while loop moves all elements underneath the one deleted to move up one space to fill the gap
            while(j < localContentNum - 1){
                strcpy(localContent[j], localContent[j+1]);
                strcpy(localPort[j], localPort[j+1]);
                tcpArr[j] = tcpArr[j+1];
                socArr[j] =  socArr[j+1];
                j++;
            }
            found = 1;
            break;
        }
    }

    // Sends an error message if the user's requested content was not found
    if(!found){
        printf("An error is found - the requested content name was not in the list:\n");
        printf("    Content Name: %s\n\n", nameOfContent);
    }else{
        localContentNum = localContentNum -1;
    }
}

void printTasks(){
    printf("Choose a task:\n");
    printf("    R: Register     ");
    printf("    T: De-register  ");
    printf("    D: Download    ");
    printf("    O: Index Server Content  ");
    printf("    L: local content    ");
    printf("    Q: Quit\n");

}

void registerContent(char nameOfContent[]){              // PDU packet to send to the index server
    struct  pdu     packetToSend,packetRead;            
    struct  pduC    contentPacket;

    char    writeMsg[101];                  // Temporary placeholder for outgoing message to index server
    int     length;                         // Length of incoming data bytes from index server
    
    //Verifying that the file exists locally
    if(access(nameOfContent, F_OK) != 0){
        // When file does not exist, it prints out a message
        printf("An error is found: File %s does not exist locally\n", nameOfContent);
        return;
    }

    // TCP Connection Variables        
    struct 	sockaddr_in server, client;
    struct  pdu     newPacket;    
    char    sendContent[sizeof(struct pduC)];
    char    tcp_host[5];
    char    t_port[6];             // Initializing the generated TCP host & port into easier called variables
    int     len;                    
    int     toRead;              
    int     m;

    // Create a TCP Stream Socket	
	if ((t = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Cannot create a TCP Socket\n");
		exit(1);
	}
    bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(0);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(t, (struct sockaddr *)&server, sizeof(server));
    len = sizeof(struct sockaddr_in);
    getsockname(t, (struct sockaddr *)&server, &len);

    snprintf(tcp_host, sizeof(tcp_host), "%d", server.sin_addr.s_addr);
    snprintf(t_port, sizeof(t_port), "%d", htons(server.sin_port));

    fprintf(stderr, "Successfully generated a TCP Socket\n");
	fprintf(stderr, "TCP socket address %s\n", tcp_host);
	fprintf(stderr, "TCP socket port %s\n", t_port);
    
    memset(&packetToSend, '\0', sizeof(packetToSend)); 
    packetToSend.type = 'R';
    strcpy(packetToSend.data,nameOfContent);
    strcat(packetToSend.data," ");
    strcat(packetToSend.data,pName);
    strcat(packetToSend.data," ");
    strcat(packetToSend.data,tcp_host);
    strcat(packetToSend.data," ");
    strcat(packetToSend.data,t_port);

    // Sends the data packet to the index server
    write(s, &packetToSend, sizeof(packetToSend));     
    memset(&packetRead,'\0',sizeof(packetRead));
    // Wait for the message from the server and check the first byte of packet to determine the PDU type (A or E)
    length = read(s, &packetRead, BUFLEN);
    
    int i = 1;
    char cTemp[10], pTemp[10];
    switch(packetRead.type){       
        case 'E':
            // Output to user
            printf("An error is found - Cannot register content:\n");
            printf("    %s\n", packetRead.data);
            printf("\n");
            break;
        case 'A':
            // Copies incoming packet into a PDU-A struct
            listen(t, 5);
            switch(fork()){
                case 0: // Child
                    // Continuously listens and uploads the file to all connecting clients available 
                    fprintf(stderr, "Content server listening for incoming connections\n");
                    while(1){
                        int client_len = sizeof(client);
                        char fileName[10];
                        newT = accept(t, (struct sockaddr *)&client, &client_len);
                        memset(&newPacket, '\0', sizeof(newPacket));
                        memset(&fileName, '\0', sizeof(fileName));
                        toRead = read(newT, &newPacket, sizeof(newPacket));
                        sscanf(newPacket.data,"%s %s",pTemp, cTemp);
                        switch(newPacket.type){
                            case 'D':
                                strcpy(fileName,cTemp);
                                //fileName[strlen(newPacket.data+10)] =  '\0';
                                fprintf(stderr, "Received the file name from the D data type:\n");
                                fprintf(stderr, "   %s\n", fileName);
                                
                                FILE *file;
                                file = fopen(fileName, "rb");
                                if(!file){
                                    fprintf(stderr, "The file %s does not exist locally\n", fileName);
                                }else{
                                    fprintf(stderr, "Content server has found the file locally, prepping to send to the client\n");
                                    contentPacket.type = 'C';
                                    while(fgets(contentPacket.content, sizeof(contentPacket.content), file) > 0){
                                        write(newT, &contentPacket, sizeof(contentPacket));
                                        fprintf(stderr, "Wrote the following contents to the client:\n %s\n", sendContent);
                                    }
                                }
                                fclose(file);
                                close(newT);
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                case -1: // Parent
                    fprintf(stderr, "An error is found when creating the child process");
                    break;
            }

            addToLocalContent(nameOfContent, t_port, t, server);
            break;
        default:
            printf("Unable to read the incoming message from the server\n\n");
    }
}

void deregisterContent(char nameOfContent[]){
    struct pdu packetToSend,packetRead;

    memset(&packetToSend, '\0', sizeof(packetToSend)); 
    packetToSend.type = 'T';
    strcpy(packetToSend.data,pName);
    strcat(packetToSend.data," ");
    strcat(packetToSend.data,nameOfContent);
   

    // The data packet is sent to the index server
    write(s, &packetToSend, sizeof(packetToSend));

    // Removing the content from the list of locally registered content
    removeFromLocalContent(nameOfContent);
    read(s, &packetRead, sizeof(packetRead));
    printf(packetRead.data);
    printf("\n");
}

void listLocalContent(){
    int j;

    printf("The list of locally registered content names:\n");
    printf("Number\t\tName\t\tPort\t\tSocket\n");
    for(j = 0; j < localContentNum; j++){
        printf("%d\t\t%s\t\t%s\t\t%d\n", j, localContent[j], localPort[j], tcpArr[j]);
    }
    printf("\n");
}

void listIndexServerContent(){
    struct pdu packetToSend,packetRead;
    
    memset(&packetToSend, '\0', sizeof(packetToSend));          // Sets terminating characters to all elements
    packetToSend.type = 'O';

    // Sends request to the Index Server
    write(s, &packetToSend, sizeof(packetToSend));

    // Reads and list out the Contents
    printf("The Index Server contains the following:\n");
    int i;
    read(s, &packetRead, sizeof(packetRead));
    
    char* tok;
    tok = strtok(packetRead.data," ");
    
    while(tok!=NULL){
        printf("%s\n",tok);
        tok = strtok(NULL," ");
    }    
}

void pingIndexFor(char nameOfContent[]){
    struct pdu packetToSend;

    // Builds the S type packet to send to the Index Server
    packetToSend.type = 'S';
    strcpy(packetToSend.data, pName);
    strcat(packetToSend.data," ");
    strcat(packetToSend.data,nameOfContent);

    // Send request to the index server
    write(s, &packetToSend, sizeof(packetToSend.type)+sizeof(packetToSend.data));
}

void downloadContent(char nameOfContent[], char address[]){     
    // TCP Connection Variables
    struct 	sockaddr_in server;
    struct	hostent		*hp;
    char    *serverHost = "0";      
    int     socDownload;      
    int     m;                          // Variable used only for iterative processes

    fprintf(stderr, "Found the needed content on index server, now preparing to download\n");

    // Create a TCP Stream Socket	
	if ((socDownload = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Cannot create a TCP socket\n");
		exit(1);
	}

    bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(address));
	if (hp = gethostbyname(serverHost)) 
        bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);
	else if ( inet_aton(serverHost, (struct in_addr *) &server.sin_addr) ){
        fprintf(stderr, "Cannot obtain the server's address\n");
	}

	// Connecting to the server 
	if (connect(socDownload, (struct sockaddr *)&server, sizeof(server)) == -1){
        fprintf(stderr, "Cannot connect to the server: %s\n", hp->h_name);
        return;
	}

    // Packet Variables        
    struct  pdu     packetToSend; 

    // Create a D-PDU to send to the Content Server
    packetToSend.type ='D';
    strcpy(packetToSend.data,pName);
    strcat(packetToSend.data," ");
    strcat(packetToSend.data,nameOfContent);
 
    // Send the request to Content Server
    write(socDownload, &packetToSend, sizeof(packetToSend.type)+sizeof(packetToSend.data));
    
    // File Download Variables
    FILE    *fp;
    struct  pduC     bufRead; 
    int     recvCon = 0;

    // Download the Data
    fp = fopen(nameOfContent, "w+");
   while ((m = read(socDownload, &bufRead, sizeof(bufRead))) > 0){
        if(bufRead.type == 'C'){
            fprintf(fp, "%s", bufRead.content);      // Writing the info received the from content server to the local file
            recvCon = 1;                  
        }else if(bufRead.type == 'E'){
            printf("Error parsing C PDU data:\n");
        }else{
            fprintf(stderr, "Received garbage from the content server, discarding it\n");
        }
    }
    fclose(fp);
    if(recvCon){
        registerContent(nameOfContent);
    }
}

int main(int argc, char **argv){
    
    // Checks the input arguments and assigns host:port connection to the server
    switch (argc) {
		case 1:
			break;
		case 2:
			host = argv[1];
            break;
		case 3:
			host = argv[1];
			port = atoi(argv[2]);
			break;
		default:
			fprintf(stderr, "USAGE: UDP Connection to Index Server [host] [port]\n");
			exit(1);
	}

    struct 	sockaddr_in sin;            

    memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;                                                                
	sin.sin_port = htons(port);

    // Generate a UDP connection to the Index Server and maps host name to the IP address
	if ( ptr = gethostbyname(host) ){
		memcpy(&sin.sin_addr, ptr->h_addr, ptr->h_length);
	}
	else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE )
		fprintf(stderr, "Cannot obtain the host entry \n");                                                               
    
    // Allocate a Socket 
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		fprintf(stderr, "Cannot create a socket \n");       

    // Connect the Socket 
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		fprintf(stderr, "Cannot connect to the host: %s\n", host);
	}
    
    printf("Hello to the P2P Network!\n");
    
    printf("Please enter a username:\n");
    while(read (0, pName, sizeof(pName)) > 10){
        printf("Sorry but that username is too long, please keep it to 9 characters or less:\n");
    }
    
    pName[strlen(pName)-1] = '\0'; 
    printf("Hello %s! ", pName);

    // Main Control Loop
    char            choiceOfUser[2];        // Stores the task from the user's choice
    char            input[10];              // Stores additional information required from the user to complete specific tasks
    struct pdu      packetRead;             // The buffer for incoming messages from the index server            
    int             flag = 0;               // Flag that is enabled when the user wants to flag the app
    int             j;                      // Used for only iterative processes
    char* tok;

    
    while(!flag){ 
        printf("Choose a task:\n");
        read(0, choiceOfUser, 2);
        memset(input, 0, sizeof(input));
        // Perform the task
        switch(choiceOfUser[0]){
            case 'R':   // Register content to the index server
                printf("Please enter a valid content name (keep it 9 characters or less):\n");
                scanf("%9s", input);      
                registerContent(input);
                break;
            case 'T':   // De-register the content
                printf("Please enter the name of the content you would like to de-register (9 characters or less):\n");
                scanf("%9s", input);   
                deregisterContent(input);
                break;
            case 'D':   // Download the content
                printf("Please enter the name of the content you would like to download (9 characters or less):\n");
                scanf("%9s", input);
                pingIndexFor(input);     // Asks the index for a specific piece of content
                
                // Reads the index answer, either an S or E type PDU
                read(s, &packetRead, sizeof(packetRead));

                switch(packetRead.type){
                    case 'E':
                        printf("An error was found when downloading content:\n");
                        printf("    %s\n", packetRead.data);
                        printf("\n");
                        break;
                    case 'S':
                        tok = strtok(packetRead.data," ");
                        tok = strtok(NULL," ");
                        tok = strtok(NULL," ");
                        downloadContent(input, tok);
                        break;
                }
                break;
            case 'O':   // List all the content available on the index server
                listIndexServerContent();
                break;
            case 'L':   // List all the local content registered
                listLocalContent();
                break;
            case 'Q':   // De-register all local content from the server and flag
                for(j = 0; j < localContentNum; j++){
                    deregisterContent(localContent[j]);
                }
                flag = 1;
                break;
            case '?':
                printTasks();
                break;
            default:
                printf("Invalid choice, please try again\n");
        } 
    }
    close(s);
    close(t);
    exit(0);
}