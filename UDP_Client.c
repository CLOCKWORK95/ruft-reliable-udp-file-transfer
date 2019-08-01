/* Client side implementation of RUFT  */ 
#include "header.h"
#include "Reliable_Data_Transfer.c"

#define PORT     49152

#define LIST    0

#define GET     1

#define PUT     2


int                     sockfd; 
char                    buffer[MAXLINE],        msg[MAXLINE]; 
struct sockaddr_in      servaddr; 


/* Initializing a pthread mutex for critical accesses on receiving window, shared by downloader and writer threads. */
pthread_mutex_t rcv_window_mutex = PTHREAD_MUTEX_INITIALIZER;


struct file_download_infos {

    pthread_t                       downloader;                                     // thread identifier of the file downloader (RDT).

    pthread_t                       writer;                                         // thread identifier of the file writer.

    rw_slot                         *rcv_wnd;                                       // download instance receiving window.

    int                             identifier;                                     // working-thread's identifier on RUFT server side.

    char                            pathname[MAXLINE];                              // client-side pathname of transcribing file.

    char                            ACK[ACK_SIZE];                                   

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



/* WRITE SIGNAL HANDLER (SIGUSR2) */

void write_sig_handler( int signo){}



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

    int ret;      char c;
      
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
  
    memset(&servaddr, 0, sizeof(servaddr)); 
      
    // Filling server information 
    servaddr.sin_family =           AF_INET; 
    servaddr.sin_port =             htons(PORT); 
    servaddr.sin_addr.s_addr =      inet_addr("127.0.0.1"); 
      
    int n, len, op; 


 ops:

    display();

    ret = scanf( "%d", &op);
    if (ret == -1)      Error_("error in function : scanf.", 1);
    while (getchar() != '\n') {}

    switch(op) {

        case LIST:     

            list_request();
            printf("\n\n Press a button to proceed...");
          
            scanf("%c", &c);
            while( getchar() != '\n'){};
            break;

        case GET:

            download_request();
            printf("\n\n File is downlading on the background from RUFT Server.\n Press a button to proceed...");

            scanf("%c", &c);
            while( getchar() != '\n'){};
            
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

    int     ret,            len = sizeof( servaddr );

    char    request[3],     answer[MAXLINE],     *list;

    sprintf( request, "0/");

    // Sending list request
    ret = sendto(sockfd, (const char *) request, MAXLINE, MSG_CONFIRM, (const struct sockaddr *) &servaddr,  sizeof(servaddr)); 
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

    char                            request[MAXLINE],        filename[MAXLINE];


    struct file_download_infos      *infos = malloc ( sizeof( struct file_download_infos ) );


    /* Set the request packet's fields. */

    printf(" Enter the file name here : ");                scanf( "%s", filename );

    sprintf( request, "1/./server_directory/%s", filename );

    printf(" Request content : %s\n\n", request); fflush(stdout);

    sprintf( ( infos -> pathname ), "./client_directory/%s", filename );


    // Sending get-request specifing the name of the file to download.
    ret = sendto( sockfd, (const char *) request, MAXLINE, MSG_CONFIRM, (const struct sockaddr *) &(servaddr),  sizeof(servaddr) ); 
    if (ret <= 0) {
        printf("Error in function : sendto (download_request). errno = %d", errno );
        return -1;
    }

    if ( pthread_create( &( infos -> downloader ), NULL, downloader, (void *) infos ) == -1 )       Error_("Error in function : pthread_create (download_request).", 1);

    if ( pthread_create( &( infos -> writer ), NULL, writer, (void *) infos ) == -1 )       Error_("Error in function : pthread_create (download_request).", 1);


    return 0;
    
}





/* THREAD FUNCTIONS IMPLEMENTATION : CLIENT SIDE's "DOWNLOAD ENVIRONMENT" */


void * downloader( void * infos_ ){

    int                             ret,            counter = 0;

    struct file_download_infos      *infos = (struct file_download_infos *) infos_;

    char                            rcv_buffer[MAXLINE];

    printf("hello 1"); fflush(stdout); sleep(2);
    // Download File.

    infos -> rcv_wnd = get_rcv_window();

    infos -> dwld_serv_len = sizeof( infos -> dwld_servaddr );

    /* Receive the file size and the identifier of server worker matched to this download instance. */
    ret = recvfrom( sockfd, (char *) rcv_buffer, MAXLINE,  MSG_WAITALL, (struct sockaddr *) &( infos -> dwld_servaddr ), &( infos -> dwld_serv_len ) ); 
    if (ret <= 0)       Error_("Error in function : recvfrom (downloader).", 1);

    printf("hello 2, fucking rcv buffer content is the following : %s", rcv_buffer ); fflush(stdout); sleep(2);

    /* Initiate the exit-condition's values for the next cycle. */
    char *idtf;
    idtf =                strtok( rcv_buffer, "/" );
    infos -> identifier = atoi( idtf );

    printf("hello 3 Server Worker ID is %d", infos -> identifier ); fflush(stdout); sleep(2);

    char *filesz;
    filesz =              strtok( NULL, "/" );

    printf("hello 4.1 Server Worker file size is %s", filesz ); fflush(stdout); sleep(2);

    int filesize =        atoi( filesz );

    printf("hello 4.2 Server Worker file size is %d", filesize ); fflush(stdout); sleep(2);

    memset( rcv_buffer, 0, MAXLINE);

    printf( "hello 5 " ); fflush(stdout); sleep(5);

    do{

        ret = recvfrom( sockfd, (char *) rcv_buffer, MAXLINE,  MSG_WAITALL, (struct sockaddr *) &( infos -> dwld_servaddr ), &( infos -> dwld_serv_len ) ); 
        if (ret <= 0)       Error_("Error in function : recvfrom (downloader).", 1);

        int identifier = atoi( ( strtok( rcv_buffer, "/") ) );

        if ( identifier == ( infos -> identifier ) ) {

            /*  THIS IS A CRITIAL SECTION FOR RECEIVING WINDOWS ACCESS ON WRITING. 
                DOWNLOADER THREAD TAKES A TOKEN FROM MUTEX TO RUN THIS CODE BLOCK. */

            if ( pthread_mutex_lock( &rcv_window_mutex ) == -1 )        Error_("Error in function : pthread_mutex_lock (downloader).", 1);

            rw_slot *wnd_tmp = ( infos -> rcv_wnd );

            int sequence_number = atoi( strtok( NULL, "/") );

            for (int i = 0; i < WINDOW_SIZE; i++) {

                if ( wnd_tmp -> sequence_number == sequence_number ) {

                    /* Send an ACKNOWLEDGMENT to the RUFT Server Side. */

                    sprintf( ( infos -> ACK ), "%d/%d/", identifier, sequence_number );

                    ret = sendto(sockfd, (const char *) ( infos -> ACK ), strlen( infos -> ACK ), MSG_CONFIRM, 
                                 (const struct sockaddr *) &( infos -> dwld_servaddr ),  sizeof( infos -> dwld_serv_len )); 
                    if (ret <= 0)       Error_("Error in function : sendto (downloader).", 1);

                    /* Update rcv_window's slot status.  */
                    wnd_tmp -> status = RECEIVED;

                    if ( sprintf( ( wnd_tmp -> packet ), "%s", ( rcv_buffer + HEADER_SIZE ) ) == -1 )        Error_( "Error in function : sprintf (downloader).", 1);

                    counter += strlen( wnd_tmp -> packet);

                    if ( ( wnd_tmp -> is_first ) == '1' ) {

                        /* If this is the first slot of the window, then alert the writer about it (SIGUSR2) so that it could slide the rcv_window on. */

                        pthread_kill( infos -> writer, SIGUSR2 );

                        if ( pthread_mutex_unlock( &rcv_window_mutex ) == -1 )        Error_("Error in function : pthread_mutex_unlock (downloader).", 1);

                    }

                    break;
                }

                wnd_tmp = wnd_tmp -> next;

            }

            if ( pthread_mutex_unlock( &rcv_window_mutex ) == -1 )        Error_("Error in function : pthread_mutex_unlock (downloader).", 1);


            /* END OF CRITICAL SECTION FOR RECEIVING WINDOW'S ACCESS. */

        }

        memset( rcv_buffer, 0, strlen(rcv_buffer) );

    } while( counter < filesize );


}


void * writer( void * infos_ ){

    /* Temporarily block SIGUSR2 signal occurrences. */
    sigset_t                                    set;
    sigemptyset( &set );
    sigaddset( &set, SIGUSR2);
    signal( SIGUSR2, write_sig_handler);
    sigprocmask( SIG_BLOCK, &set, NULL );

    int                                         ret,            file_descriptor,            counter = 0;

    struct file_download_infos                  *infos = ( struct file_download_infos * ) infos_;

    /* Create the new file in client's directory or truncate an existing one with the same pathname, to start download. */
    file_descriptor = open( ( infos -> pathname ), O_WRONLY | O_CREAT | O_TRUNC );  
    if (file_descriptor == -1)      Error_("Error in function : open (writer).", 1);

    do{
        /* Be ready to be awaken by SIGUSR2 occurrence. Go on pause. */
        sigprocmask( SIG_UNBLOCK, &set, NULL );

        pause();

        /* Temporarily block SIGUSR2 signal occurrences. */
        sigprocmask( SIG_BLOCK, &set, NULL );

        printf( "here i am awaked (writer)" ); sleep(1);

        {   
            /*  THIS IS A CRITIAL SECTION FOR RECEIVING WINDOWS ACCESS ON WRITING. 
                WRITER THREAD TAKES A TOKEN FROM MUTEX TO RUN THIS CODE BLOCK. */

            if ( pthread_mutex_lock( &rcv_window_mutex ) == -1 )        Error_("Error in function : pthread_mutex_lock (writer).", 1);
        
            rw_slot      *wnd_tmp = ( infos -> rcv_wnd );

            rw_slot      *curr_first   = ( infos -> rcv_wnd );

            while ( wnd_tmp -> status == RECEIVED ) {
                
                /* Write the packet within the new file in client's directory. */
                ret = write( file_descriptor, ( wnd_tmp -> packet ), PACKET_SIZE );
                if ( ret == -1)         Error_( "Error in function : write (thread writer).", 1);

                counter ++;

                wnd_tmp = ( wnd_tmp -> next );

            }

            
            while ( curr_first -> status == RECEIVED ){

                /* Slide the receiving window on. */

                ( curr_first -> sequence_number ) += WINDOW_SIZE;
                ( curr_first -> status ) == WAITING;
                memset( ( curr_first -> packet ), 0, PACKET_SIZE );
                curr_first -> is_first = '0';

                curr_first -> next -> is_first = '1';
                curr_first = ( curr_first -> next );
                infos -> rcv_wnd = curr_first;

            }

            if ( pthread_mutex_unlock( &rcv_window_mutex ) == -1 )        Error_("Error in function : pthread_mutex_unlock (writer).", 1);

            /* END OF THE CRITICAL SECTION. */

        }



    } while (1);

}