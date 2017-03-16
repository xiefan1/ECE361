//Author: Fan Xie
//Author: Karen Cerullo
//2017-03-15
// TEXT CONFERENCING LAB - CLIENT - PART 1 (LAB 4)

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MAX_SIZE 1024
#define FIELD_SIZE 10
#define CMD_SIZE 20

////////////////////////////////////////////////////////////////////////////////

struct messageStruct {
    unsigned char type[FIELD_SIZE];         // Type of message from table (ie. LOGIN, EXIT, etc)
    unsigned int size;              // Length of the data (Bytes)
    unsigned char source[FIELD_SIZE];       // ID of the client sending the message
    unsigned char data[MAX_SIZE];       // Data being sent (Use colon as delimiter)
};

////////////////////////////////////////////////////////////////////////////////

void error(const char *msg) {
    //sends the message as a system error
    perror(msg);
    //terminates program
    exit(0);
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
    
    //VARIABLES
    
    int sock=-1; //socket
    struct sockaddr_in server; //server
    struct sockaddr_in client; //client
    struct hostent *host; //host
    unsigned int length; //size of struct
    char buffer[256]; //holds message
    int n; //number of bytes
    
    char command[CMD_SIZE];
    char clientID[FIELD_SIZE];
    char password[FIELD_SIZE];
    char serverIP[30];
    char portNumber[FIELD_SIZE];
    char messageBuffer[MAX_SIZE];
    char ackString[MAX_SIZE];
    int m;
    char sessionID[FIELD_SIZE] = "0";
    bool loggedIn = false;  
    
    // START THE CLIENT PROGRAM
    
    int sockfd, numbytes;
    char buf[MAX_SIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    
    // SELECT VARIABLES
    fd_set readfds; // Set of file discriptors (things that will interrupt)
    //int max_sockfd; // highest-numbered descriptor (sock)
    
    printf("CLIENT\n");
           
    // CLIENT LOOP
    
    while (1) {
    	
	     // HANDLE SELECT
	     FD_ZERO(&readfds); // Clear the list
	     FD_SET(fileno(stdin), &readfds); // Add stdin to the list	

	     if (sock>0) {
	          FD_SET(sock, &readfds); // Add sock to the list
		  select(sock+1,&readfds,NULL,NULL,NULL);    	
	     }else{
	     	  select(fileno(stdin)+1,&readfds,NULL,NULL,NULL);    	
	     }
        
        // IF SOCK (RECV) - PRINT MESSAGE
        if(loggedIn && FD_ISSET(sock,&readfds)){
             // print message
	     char buf[1024]={0};
	     
	     recv(sock,buf,1024,0);
	     printf("%s\n",buf+8);
        }
        else if (FD_ISSET(fileno(stdin),&readfds)){
    
        // IF STDIN - DO COMMANDS        
        // Get Command
        scanf("%s",command);
    
        // Handle Command
        
        ////////////////////////////////////////////////////////////////////////
        
        if (strcmp(command, "/login") == 0) {
            
            if (loggedIn) {
                printf("   Already Logged In\n");
            }
            
            else {                                              
        
                // GET LOGIN INFORMATION
           
                scanf(" %s %s %s %s", clientID, password, serverIP, portNumber);
          
               // CREATE A SOCKET

                memset(&hints, 0, sizeof hints);
                hints.ai_family = AF_UNSPEC;
                hints.ai_socktype = SOCK_STREAM;
		if ((rv = getaddrinfo(serverIP, portNumber, &hints, &servinfo)) != 0) {
                    printf("   Error: Getting Address Info\n");
                    return 1;                           
                }
                
                // Loop through to connect
                for (p = servinfo; p != NULL; p = p->ai_next) {
                    if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                        printf("   Error: Client Socket\n");        
                        continue;
                    }
                    if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
                        printf("   Error: Client Connect\n");
                        continue;
                    }
                    break;
                }
                
                if (p == NULL) {
                    printf("   Error: Client Failed To Connect\n");
                }
                
                // CONNECTED - SET MASTER_FD
   
                
                
                bzero(messageBuffer,MAX_SIZE);
                sprintf(messageBuffer, "%s:%lu:%s:%s", "LOGIN",strlen(password),clientID,password);
        
                printf("\n   MESSAGE BUFFER\n");
                printf("   ~%s~\n\n", messageBuffer);

                // SEND LOGIN MESSAGE TO SERVER
                n = send(sock, messageBuffer, strlen(messageBuffer), 0);
        
                // RECEIVE LO_ACK OR LO_NAK FROM SERVER
              
                bzero(ackString, MAX_SIZE);
                m = recv(sock, ackString, MAX_SIZE, 0);
                        
                char LO_ACK[6] = "LO_ACK";
        
                // Check if LO_ACK or LO_NAK
                int x;
                for (x=0; x<6; x++) {
                    if (ackString[x] == LO_ACK[x])
                        loggedIn = true;
                    else {
                        loggedIn = false;
                        x = 6; 
                    }
                }
        
                // If login failed - print error and loop again
                if (!loggedIn) {
                    printf("   Login Failed: %s\n", ackString);
                } 

            }
            
        }
    
        ////////////////////////////////////////////////////////////////////////
        
        else if (strcmp(command, "/logout") == 0) {
            
            if (loggedIn) {
                
                loggedIn = false;
        
                sprintf(messageBuffer, "%s:%d:%s:%s", "EXIT",0,clientID,"");
                
                //printf("\n   MESSAGE BUFFER\n");
                //printf("   ~%s~\n\n", messageBuffer);
        
                n = send(sock, messageBuffer, strlen(messageBuffer), 0);
                close(sock);                
		sock=-1;
                printf("   Logged Out\n");
            }
            
            else {
                printf("   Not Logged In Yet\n");
            }
        
        }
    
        ////////////////////////////////////////////////////////////////////////
        
        else if (strcmp(command, "/joinsession") == 0) {
            
            if (loggedIn) {
                
                    scanf(" %s", sessionID);
        
                    sprintf(messageBuffer, "%s:%lu:%s:%s", "JOIN",strlen(sessionID),clientID,sessionID);
                    
                    //printf("\n   MESSAGE BUFFER\n");
                    //printf("   ~%s~\n\n", messageBuffer);
        
                    n = send(sock, messageBuffer, strlen(messageBuffer), 0);
                    
        
                    // Receive JN_ACK or JN_NAK
                    bzero(ackString, MAX_SIZE);
                    m = recv(sock, ackString, MAX_SIZE, 0);
                    
                   // Check if JN_ACK or JN_NAK
                    char JN_ACK[7] = "JN_ACK";
                    
                    int x;

                    
                    if (strcmp(ackString,JN_ACK)!=0) {
                        printf("   Joining Session Failed: %s\n", ackString);
                    }
                    else {
                        printf("   Successfully Joined Session %s\n", sessionID);
                    }
                
                }
            else {
                printf("   Not Logged In Yet\n");
            }
        }
    
        ////////////////////////////////////////////////////////////////////////
        
        else if (strcmp(command, "/leavesession") == 0) {
            
            if (loggedIn) {
                
                    
                    scanf(" %s", sessionID);
                         
                    sprintf(messageBuffer, "%s:%lu:%s:%s", "LEAVE_SESS",strlen(sessionID),clientID,sessionID);
                    
                    //printf("\n   MESSAGE BUFFER\n");
                    //printf("   ~%s~\n\n", messageBuffer);
        
                    n = send(sock, messageBuffer, strlen(messageBuffer), 0);
                    
            
                    printf("   Left Session %s\n", sessionID);
            
            }
            
            else {
                printf("   Not Logged In Yet\n");
            }
        }
    
        ////////////////////////////////////////////////////////////////////////
        
        else if (strcmp(command, "/createsession") == 0) {

            sprintf(messageBuffer, "%s:%d:%s:%s", "NEW_SESS",0,clientID,"");
            
//            printf("\n   MESSAGE BUFFER\n");
//            printf("   ~%s~\n\n", messageBuffer);
            
            n = send(sock, messageBuffer, strlen(messageBuffer), 0);
            
            if (n < 0) {
                perror("\nCreate Session\n");
            }
            
            bzero(ackString, MAX_SIZE);
            m = recv(sock, ackString, MAX_SIZE, 0);
            
            
            char NS_ACK[6] = "NS_ACK";
                    
            int x;
            bool created = false;
            for (x=0; x<6; x++) {
                if (ackString[x] == NS_ACK[x])
                    created = true;
                else {
                    created = false;
                    x = 6; 
                }
            }
 
            if (created) {
                printf("   Successfully Joined Session ~%c", ackString[7]);
            
                if (ackString[8] != '\0')
                    printf("%c~\n", ackString[8]);
                else
                    printf("~\n");
            }
            else {
                printf("   Session Not Created: %s\n", ackString);
            }
 
            
                          
        }
        
        ////////////////////////////////////////////////////////////////////////
    
        else if (strcmp(command, "/list") == 0) {
            
            if (loggedIn) {
        
                sprintf(messageBuffer, "%s:%d:%s:%s", "QUERY",0,clientID,"");
                
                n = send(sock, messageBuffer, strlen(messageBuffer), 0);
            /*    printf("\n\nflag1\n\n");
                
                // Receive ACK containing list of users and sessions
                bzero(ackString, MAX_SIZE);
                m = recv(sock, ackString, MAX_SIZE, 0);
              
                printf("\n\nflag2\n\n");
            */   
                // TEST LIST COMMAND WITH THIS STRING - WORKS
//                char ackString[33] = "Karen:100:Fan:2:Gaga:NULL:Lady:55";

                  char ackString[MAX_SIZE];
                 // Receive ACK containing list of users and sessions
                bzero(ackString, MAX_SIZE);
                m = recv(sock, ackString, MAX_SIZE, 0);
              
		 printf("%s\n",ackString+7);
                
/*
                int ackIndex;
                int fieldIndex = 0;
                char field[MAX_SIZE];
                int userVSsess = 0; // user = 0; session = 1;
                for (ackIndex=0; ackIndex< strlen(ackString); ackIndex++) {
                    // If not at a colon
                    if (ackString[ackIndex] != ':') {
                        field[fieldIndex] = ackString[ackIndex];
                        fieldIndex = fieldIndex + 1;
                    }
                    // If at a colon
                    else if (ackString[ackIndex] == ':') {
                        // Display user
                        if (userVSsess == 0) {
                            printf("\n   User: %s -> ", field);       
                            userVSsess = 1;                           
                        }
                        // Display session
                        else {
                            printf("Session: ~%s~", field);
                            userVSsess = 0;
                        } 
                        bzero(field,MAX_SIZE);
                        fieldIndex = 0;
                    }
                }
                
                // Last session
                printf("Session: ~%s~\n\n", field);
         */                       
            }
            
            else {
                printf("   Not Logged In Yet\n");
            }
            
        }
        
        ////////////////////////////////////////////////////////////////////////
    
        else if (strcmp(command, "/quit") == 0) {
        
	    send(sock,"EXIT::",5,0);
            printf("\n   Quitting Text Conferencing Application\n\n");
            close(sock);
            sock=-1;
        
            return (EXIT_SUCCESS);
        }
        
        ////////////////////////////////////////////////////////////////////////
    
        else {
            
            // Message
            
            if (loggedIn) {
                
                    // Get all text in the terminal and send that
		  char msg[1024]={0};
		  strcpy(msg,command);
		  int off=strlen(command);
  		  fgets(msg+off, 1024-off, stdin);
		  char buf[1300]={0};
		  int bytes=sprintf(buf,"MESSAGE:%lu:%s:%s",strlen(msg),clientID,msg);

                   send(sock,buf, strlen(buf),0); 
            }
            else {
                printf("   Not Logged In Yet (message) \n");
            }
        }
        
        ////////////////////////////////////////////////////////////////////////
        
      } // END OF STDIN 
    
    }
       
    ////////////////////////////////////////////////////////////////////////////
    
    return (EXIT_SUCCESS);
}

