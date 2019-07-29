/* Client side implementation of RUFT  */ 
#include "header.h"
#include "Reliable_Data_Transfer.c"

#define PORT     8080 

int                     sockfd; 
char                    buffer[MAXLINE],		up_buffer[MAXLINE],        msg[MAXLINE]; 
struct sockaddr_in      servaddr; 





/* CLIENT AVAILABLE REQUESTS DECLARATION */

int list_request();

int download_request();

int upload_request();





/* This is the Client's GUI to communicate with RUFT Server size. */
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

    printf(" Please, write the operation code : ");


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

    ret = scanf( "%d", &op);
    if (ret == -1)      Error_("error in function : scanf.", 1);
    while (getchar() != '\n') {}

    switch(op) {

        case 0:
            /* LIST */
            list_request();
            printf("\n\nPress a button to proceed...");

            char c;
            scanf("%c", &c);
            while( getchar() != '\n'){};
            break;

        case 1:
            /* GET */
            download_request();
            printf("\n\nPress a button to proceed...");

            char c;
            scanf("%c", &c);
            while( getchar() != '\n'){};
            break;

        case 2:
            /* PUT */
            upload_request();
            printf("\n\nPress a button to proceed...");

            char c;
            scanf("%c", &c);
            while( getchar() != '\n'){};
            break;

        default:
            break;

    }

    goto ops;
    
  
    close(sockfd); 

    return 0; 

} 



int list_request() {

    int     ret,    len;

    char    request[3],     answer[MAXLINE],     *list;

    sprintf( request, "0/");

    // Sending list request
    ret = sendto(sockfd, (const char *) request, strlen(request), MSG_CONFIRM, (const struct sockaddr *) &servaddr,  sizeof(servaddr)); 
    if (ret <= 0) {
        printf("Error in function : sendto (list_request).");
        return -1;
    }

    // Receiving confirm and size of the list.
    ret = recvfrom( sockfd, (char *) buffer, MAXLINE,  MSG_WAITALL, (struct sockaddr *) &servaddr, &len ); 
    if (ret <= 0) {
        printf("Error in function : recvfrom (list_request).");
        return -1;
    }

    ret = atoi(buffer);         list = malloc( sizeof(char) * ret );


    // Receiving the list.
    ret = recvfrom( sockfd, (char *) list, MAXLINE,  MSG_WAITALL, (struct sockaddr *) &servaddr, &len ); 
    if (ret <= 0) {
        printf("Error in function : recvfrom (list_request).");
        return -1;
    }

    printf( "\nSERVER DIRECTORY CONTENTS ARE :\n\n%s\n\n", list );

    free( list );

    return 0;
    
}




int download_request() {

    int                     ret,            len,            dwld_serv_len;

    struct sockaddr_in      dwld_servaddr;       

    char                    request[MAXLINE],       pathname[MAXLINE],        *filename;

    /* Set the request packet's fields. */

    sprintf( request, "1/");

    printf("Enter the file name here : ");

    scanf( "%s", filename );

    sprintf( ( request + strlen( request ) ), "./server_directory/%s", filename );

    sprintf( pathname, "./client_directory/%s", filename );


    // Create the new file in client's directory and open a writing session on it.
    int fd = open( pathname, O_WRONLY | O_CREAT | O_TRUNC );


    // Sending download request.
    ret = sendto(sockfd, (const char *) request, strlen(request), MSG_CONFIRM, (const struct sockaddr *) &servaddr,  sizeof(servaddr)); 
    if (ret <= 0) {
        printf("Error in function : sendto (download_request).");
        return -1;
    }

    // Download File.
    do{

        ret = recvfrom( sockfd, (char *) buffer, MAXLINE,  MSG_WAITALL, (struct sockaddr *) &dwld_servaddr, &dwld_serv_len ); 
        if (ret <= 0) {
            printf("Error in function : recvfrom (list_request).");
            return -1;
        }

    } while(1);


    return 0;
    
}

/* Every client can make a request to send more files, but it must specify the number of files to be sent */

int upload_request() {

	int                     ret,            len,            upld_serv_len, 			filenum;

    struct sockaddr_in      upld_servaddr;       

    char                   	request[MAXLINE],       pathname[MAXLINE],        *filename;
   
    /* Set the request packet's fields. */

    sprintf( request, "2/");

	printf("Enter the number of files to send : ");
	
	scanf( "%d", filenum );

    sprintf( ( request + strlen( request ) ), "%d", filenum );  //adds the number of files after the request number
    //on server side, filenum files will be created in the server directory before the 'ok'
    
    
    // Sending upload request.
    ret = sendto( sockfd, (const char *) request, strlen(request), MSG_CONFIRM, (const struct sockaddr *) &servaddr,  sizeof(servaddr)); 
    if (ret <= 0) {
        printf("Error in function : sendto (upload_request).");
        return -1;
    }
    
    // Receiving upload permission. (The files have been created successfully on server and are ready to be written)
    ret = recvfrom( sockfd, (char *) buffer, MAXLINE,  MSG_WAITALL, (struct sockaddr *) &servaddr, &len ); 
    if (ret <= 0) {
        printf("Error in function : recvfrom (list_request).");
        return -1;
    }

    // Upload File.
    
    //start_upload(filenum);

	return 0;
}

