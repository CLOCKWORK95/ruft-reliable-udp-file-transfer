/* Client side implementation of RUFT  */ 
#include "header.h"
#include "Reliable_Data_Transfer.c"

#define PORT     8080 

#define LIST    0

#define GET     1

#define PUT     2


int                     sockfd; 
char                    buffer[MAXLINE],        msg[MAXLINE]; 
struct sockaddr_in      servaddr; 


struct file_download_infos {

    pthread_t                       downloader;                                     // thread identifier of the file downloader (RDT).

    pthread_t                       writer;                                         // thread identifier of the file writer.

    rw_slot                         *rcv_wnd;                                       // download instance receiving window.

    int                             identifier;                                     // working-thread's identifier on RUFT server side.

    char                            pathname[MAXLINE];                              // client-side pathname of transcribing file.

    char                            ACK[MAXLINE];

    struct sockaddr_in              dwld_servaddr;

    int                             dwld_serv_len;

};


/* THREAD FUNCTIONS DECLARATION */

void * downloader( void * infos);

void * writer( void * infos);




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

        case LIST:     

            list_request();
            printf("\n\nPress a button to proceed...");

            char c;
            scanf("%c", &c);
            while( getchar() != '\n'){};
            break;

        case GET:

            break;

        case PUT:

            break;

        default:
            break;

    }

    goto ops;
    
  
    close(sockfd); 

    return 0; 

} 







/* REQUEST FUNCTIONS IMPLEMENTATION */


/* This function implements the list request. */
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

    int                             ret,                     len;

    char                            request[MAXLINE],        *filename;


    struct file_download_infos      *infos = malloc ( sizeof( struct file_download_infos ) );


    /* Set the request packet's fields. */

    sprintf( request, "1/");

    printf("Enter the file name here : ");                scanf( "%s", filename );

    sprintf( ( request + strlen( request ) ), "./server_directory/%s", filename );

    sprintf( ( infos -> pathname ), "./client_directory/%s", filename );


    // Sending get-request specifing the name of the file to download.
    ret = sendto( sockfd, (const char *) request, strlen(request), MSG_CONFIRM, (const struct sockaddr *) &(servaddr),  sizeof(servaddr) ); 
    if (ret <= 0) {
        printf("Error in function : sendto (download_request).");
        return -1;
    }

    if ( pthread_create( &( infos -> downloader ), NULL, downloader, (void *) infos ) == -1 )       Error_("Error in function : pthread_create (download_request).", 1);

    if ( pthread_create( &( infos -> writer ), NULL, writer, (void *) infos ) == -1 )       Error_("Error in function : pthread_create (download_request).", 1);



    /*  Receiving confirm of begin transaction or error packet.
    ret = recvfrom( sockfd, (char *) buffer, MAXLINE,  MSG_WAITALL, (struct sockaddr *) &servaddr,  &len ); 
    if (ret <= 0) {
        printf("Error in function : recvfrom (list_request).");
        return -1;
    } */

    


    return 0;
    
}



void * downloader( void * infos_ ){

    int                             ret;

    struct file_download_infos      *infos = (struct file_download_infos *) infos_;

    char                            rcv_buffer[MAXLINE];


    // Download File.

    infos -> rcv_wnd = get_rcv_window();

    do{

        ret = recvfrom( sockfd, (char *) rcv_buffer, MAXLINE,  MSG_WAITALL, (struct sockaddr *) &( infos -> dwld_servaddr ), &( infos -> dwld_serv_len ) ); 
        if (ret <= 0) {
            printf("Error in function : recvfrom (list_request).");
            return -1;
        }

        int identifier = atoi( ( strtok( rcv_buffer, "/") ) );

        if ( identifier == ( infos -> identifier ) ) {

            rw_slot *wnd_tmp = ( infos -> rcv_wnd );

            int sequence_number = atoi( strtok( NULL, "/") );

            for (int i = 0; i < WINDOW_SIZE; i++) {

                if ( wnd_tmp -> sequence_number == sequence_number ) {

                    if ( sprintf( ( wnd_tmp -> packet), "%s", ( rcv_buffer + HEADER_SIZE ) ) == -1 )        Error_( "Error in function : sprintf (downloader).", 1);

                    if ( ( wnd_tmp -> is_first ) == '1' )       pthread_kill( infos -> writer, SIGUSR2 );

                    // SLIDE THE WINDOW!!!! AND CLOSE THE CYCLE!!!! AND MAKE THE WRITER TASK!!!!

                    break;
                }

            }








        }


    } while(1);





}


void * writer( void * infos ){

}