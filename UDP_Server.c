// Server side implementation of UDP client-server model : RECEPTION ENVIRONMENT
#define _POSIX_SOURCE
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include "support.c"
#include "Reliable_Data_Transfer.c"
#include "Download_Environment.c"
  
#define PORT        8080 
#define MAXLINE     1024

int                     sockfd; 
char                    buffer[MAXLINE],       msg[MAXLINE];
struct sockaddr_in      servaddr,              cliaddr;
  

int main(int argc, char ** argv) { 

     int ret;
      
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
      
    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr)); 
      
    // Filling server information 
    servaddr.sin_family    = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(PORT); 
      
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
      
    int len, n; 

    do{
       
        printf("\n\nserver is waiting for msgs to echo...\n");

        // Receiving a msg.
        n = recvfrom(sockfd, (char *) buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len); 
        buffer[n] = '\0'; 
        printf("Client : %s\n", buffer); 

        // Echoing received msg.
        sendto(sockfd, (const char *) buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len); 
        printf("Echo sent : %s\n", buffer); 
        
        memset( buffer, 0, strlen(buffer) );


    } while (1);

     
      
    return 0; 
} 
