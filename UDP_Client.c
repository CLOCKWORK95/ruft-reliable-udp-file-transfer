// Client side implementation of UDP client-server model 
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
  
#define PORT     8080 
#define MAXLINE 1024 

int                     sockfd; 
char                    buffer[MAXLINE],        msg[MAXLINE]; 
struct sockaddr_in      servaddr; 



void display() {

    printf("\e[1;1H\e[2J");
    printf("....................................................................................\n");
    printf("....................................................................................\n");
	printf("..................|      RUFT - Reliable UDP File Transfer     |....................\n");
    printf("....................................................................................\n");
	printf("....................................................................................\n\n");

	printf(" _____ ________ ________ _______select an operation________ ________ _______ _______\n");
	printf("|                                                                                   |\n");
    printf("|   OP   0 :    list.                                                               |\n");
    printf("|   OP   1 :    get.                                                                |\n");
    printf("|   OP   2 :    put.                                                                |\n");
	printf("|____ ________ ________ ________ ________ ________ ________ ________ ________ ______|\n\n");


}

  

int main(int argc, char** argv) { 

    int ret;
      
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
  
    memset(&servaddr, 0, sizeof(servaddr)); 
      
    // Filling server information 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(PORT); 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
      
    int n, len, op; 


ops:

    display();

    printf("\n\nplease write the op code : ");

    ret = scanf( "%d", &op);
    if (ret == -1)      Error_("error in function : scanf.", 1);
    while (getchar() != '\n') {}

    switch(op) {

        case 0:
            break;

        case 1:
            break;

        case 2:
            break;

        default:

            memset(msg, 0, strlen(msg));
            printf("please write a message: ");

            ret = scanf("%s", msg);
            if (ret == -1)      Error_("error in function : scanf.", 1);
            while (getchar() != '\n') {}

            // Sending hello msg.
            sendto(sockfd, (const char *) msg, strlen(msg), MSG_CONFIRM, (const struct sockaddr *) &servaddr,  sizeof(servaddr)); 
            printf("message sent.\n"); 

            // Receiving aswer.
            n = recvfrom(sockfd, (char *)buffer, MAXLINE,  MSG_WAITALL, (struct sockaddr *) &servaddr, &len); 

            buffer[n] = '\0';
            printf("Server : %s.        (press a button to proceed...)\n", buffer); 

            char end[1];
            scanf("%s", end);

            break;

    }

    goto ops;
    
    
  
    close(sockfd); 

    return 0; 

} 
