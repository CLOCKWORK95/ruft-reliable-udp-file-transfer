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


    // Download File.

    printf("\n PREPEARING TO DOWNLOAD...\n Getting informations about the downloading file...");        fflush(stdout);

    infos -> rcv_wnd = get_rcv_window();

    infos -> dwld_serv_len = sizeof( struct sockaddr_in );

    /* Receive the file size and the identifier of server worker matched to this download instance. */
    ret = recvfrom( sockfd, (char *) rcv_buffer, MAXLINE,  MSG_WAITALL, (struct sockaddr *) &( infos -> dwld_servaddr ), &( infos -> dwld_serv_len ) ); 
    if (ret <= 0)       Error_("Error in function : recvfrom (downloader).", 1);

    printf("\n RECEIVED infos : %s", rcv_buffer );                                                      fflush(stdout); 

    /* Initiate the exit-condition's values for the next cycle. */
    char *idtf;
    idtf =                strtok( rcv_buffer, "/" );
    infos -> identifier = atoi( idtf );

    printf("\n - WORKER ID : %d", infos -> identifier );                                                fflush(stdout); 

    char *filesz;
    filesz =              strtok( NULL, "/" );
    int filesize =        atoi( filesz );

    printf("\n - SIZE : %d", filesize );                                                                fflush(stdout); 

    memset( rcv_buffer, 0, MAXLINE);

    do{

        printf("\n  DOWNLOAD IN PROGRESS... ");                                                         fflush(stdout);

        ret = recvfrom( sockfd, (char *) rcv_buffer, MAXLINE,  MSG_WAITALL, 
                        (struct sockaddr *) &( infos -> dwld_servaddr ), &( infos -> dwld_serv_len ) ); 
        if (ret <= 0)       Error_("Error in function : recvfrom (downloader).", 1);

        char    *idtf;                          idtf = strtok( rcv_buffer, "/");
        int     identifier = atoi( idtf );

        
        if ( identifier == ( infos -> identifier ) ) {

            /*  THIS IS A CRITIAL SECTION FOR RECEIVING WINDOWS ACCESS ON WRITING. 
                DOWNLOADER THREAD TAKES A TOKEN FROM MUTEX TO RUN THIS CODE BLOCK. */

            if ( pthread_mutex_lock( &rcv_window_mutex ) == -1 )        Error_("Error in function : pthread_mutex_lock (downloader).", 1);

            rw_slot     *wnd_tmp = ( infos -> rcv_wnd );

            char        *sn = strtok( NULL, "/" );

            int         sequence_number = atoi( sn );

            printf("\n --> Arrived packet with sequence number : %d .", sequence_number);               fflush(stdout);

            for (int i = 0; i < WINDOW_SIZE; i++) {

                printf("\n wnd_tmp->sequence_number=%d  sequence_number=%d", wnd_tmp->sequence_number,sequence_number); fflush(stdout);

                if ( wnd_tmp -> sequence_number == sequence_number ) {

                    /* Send an ACKNOWLEDGMENT to the RUFT Server Side. */

                    sprintf( ( infos -> ACK ), "%d/%d/", identifier, sequence_number );

                    printf(" SENDING ACK : %s", infos -> ACK);

                    ret = sendto( sockfd, (const char *) ( infos -> ACK ), MAXLINE, MSG_CONFIRM, 
                                 (const struct sockaddr *) &( infos -> dwld_servaddr ), infos -> dwld_serv_len ); 
                    if (ret <= 0) {
                        printf("\n Error in function : sendto (downloader). errno = %d ", errno );
                        exit(-1);
                    }

                    /* Update rcv_window's slot status.  */
                    wnd_tmp -> status = RECEIVED;

                    wnd_tmp -> packet = malloc( sizeof(char) * MAXLINE );
                    if (wnd_tmp -> packet == NULL)      Error_( "Error in function : malloc (downloader).", 1);
                    if ( sprintf( ( wnd_tmp -> packet ), "%s", ( rcv_buffer + ( strlen(idtf) + strlen(sn) + 2 ) ) ) == -1 )        
                                                                         Error_( "Error in function : sprintf (downloader).", 1);

                    counter += strlen( wnd_tmp -> packet);

                    if ( ( wnd_tmp -> is_first ) == '1' ) {

                        printf( "\n SENDING SIGNAL TO WRITER FOR PACKET %d", sequence_number );      fflush(stdout); 

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

        printf("\n counter = %d, filesize = %d", counter, filesize);  fflush(stdout);

        memset( rcv_buffer, 0, strlen(rcv_buffer) );

    } while( counter < filesize );


}


void * writer( void * infos_ ){

    /* Temporarily block SIGUSR2 signal occurrences. */
    sigset_t    set;
    sigemptyset( &set );
    sigaddset( &set, SIGUSR2);
    sigprocmask( SIG_BLOCK, &set, NULL );

    int                                         ret,            file_descriptor,            counter = 0;

    struct file_download_infos                  *infos = ( struct file_download_infos * ) infos_;

    /* Create the new file in client's directory or truncate an existing one with the same pathname, to start download. */
    file_descriptor = open( ( infos -> pathname ), O_RDWR | O_CREAT | O_TRUNC, 0660 );  
    if (file_descriptor == -1) {
        printf("\n Error in function : open (writer). errno = %d", errno);
        pthread_exit(NULL);
    }

    printf("\n WRITER IS ENTERING THE CYCLE.");                            fflush(stdout);

    do {
        printf("\n WRITER IS in THE CYCLE.");                            fflush(stdout);
        /* Be ready to be awaken by SIGUSR2 occurrence. Go on pause. */
        
        if( sigpending( &set ) == -1 ){
            printf("\n Error in function : sigpending (writer). errno = %d", errno);
            pthread_exit(NULL);
        }

        if ( sigismember( &set, SIGUSR2 ) ) {
            signal( SIGUSR2, write_sig_handler );
            sigprocmask( SIG_UNBLOCK, &set, NULL );
            printf("\n SIGUSR2 pending on mask! goto action.");            fflush(stdout);
            goto action;
        }

        signal( SIGUSR2, write_sig_handler );
        sigemptyset( &set );
        sigaddset( &set, SIGUSR2);
        sigprocmask( SIG_UNBLOCK, &set, NULL );

        printf("\n SIGUSR2 UNBLOCKED");                                    fflush(stdout);

        pause();

        action:
        
        printf("\n SIGUSR2 BLOCKED");                                      fflush(stdout);

        /* Temporarily block SIGUSR2 signal occurrences. */
        sigprocmask( SIG_BLOCK, &set, NULL );

        printf( "\n WRITER AWAKED" );                                      fflush(stdout);

        {   
            /*  THIS IS A CRITIAL SECTION FOR RECEIVING WINDOWS ACCESS ON WRITING. 
                WRITER THREAD TAKES A TOKEN FROM MUTEX TO RUN THIS CODE BLOCK. */

            if ( pthread_mutex_lock( &rcv_window_mutex ) == -1 )        Error_("Error in function : pthread_mutex_lock (writer).", 1);
        
            rw_slot      *wnd_tmp = ( infos -> rcv_wnd );

            rw_slot      *curr_first   = ( infos -> rcv_wnd );

            while( ( curr_first -> status != RECEIVED ) && ( curr_first -> is_first  != '1') ) {
                curr_first = ( curr_first -> next );
            }

            while ( curr_first -> status == RECEIVED ) {

                lseek( file_descriptor, 0, SEEK_END );

                /* Write the packet within the new file in client's directory. */
                ret = write( file_descriptor, ( curr_first -> packet ), strlen( curr_first -> packet  ) );
                if ( ret == -1)         Error_( "Error in function : write (thread writer).", 1);

                printf( "\n Packet %d content has been written on file %s. %d bytes written .", 
                                            ( curr_first -> sequence_number ), ( infos -> pathname ), ret );            fflush(stdout);


                /* Slide the receiving window on. */

                ( curr_first -> sequence_number ) += WINDOW_SIZE;
                ( curr_first -> status ) = WAITING;
                memset( ( curr_first -> packet ), 0, sizeof( curr_first -> packet ) );
                curr_first -> is_first = '0';

                curr_first -> next -> is_first = '1';
                curr_first = ( curr_first -> next );
                infos -> rcv_wnd = curr_first;

                printf("\n WINDOW SLIDED ON");      fflush(stdout);
                
            
            }


            if ( pthread_mutex_unlock( &rcv_window_mutex ) == -1 )        Error_("Error in function : pthread_mutex_unlock (writer).", 1);

            /* END OF THE CRITICAL SECTION. */

        }



    } while (1);

}