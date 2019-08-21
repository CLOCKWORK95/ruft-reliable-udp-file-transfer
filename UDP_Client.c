/* Client side implementation of RUFT  */ 
#include "header.h"
#include "Reliable_Data_Transfer.c"

#define PORT     5193

#define LIST    0

#define GET     1

#define PUT     2


int                     sockfd; 
char                    buffer[MAXLINE],        msg[MAXLINE]; 
struct sockaddr_in      servaddr; 
struct sockaddr_in      upload_serv_addr;         int    upload_addr_len = sizeof( struct sockaddr_in );


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

    char                            finish;

};



/* THREAD FUNCTIONS DECLARATION */

void * downloader( void * infos);

void * writer( void * infos);


/* CLIENT AVAILABLE REQUESTS DECLARATION */

int list_request();

int download_request();

int upload_request();

int initialize_upload_instance();



/* WRITE SIGNAL HANDLER (SIGUSR2) */

void write_sig_handler( int signo ) {}



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

            upload_request();
            printf("\n\n Press a button to proceed...");
          
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




/* REQUEST FUNCTIONS IMPLEMENTATION */


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

    char                            request[2 * MAXLINE],        filename[MAXLINE];


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



int upload_request() {

    int                             ret;

    char                            request[2 * MAXLINE],        filename[MAXLINE],     filetoupload[MAXLINE];


    /* Set the request packet's fields. */

    printf(" Enter the file name here : ");                                                     scanf( "%s", filename );

    sprintf( request, "2/./server_directory/%s", filename );

    printf(" Request content : %s\n\n", request);                                               fflush(stdout);

    sprintf( filetoupload, "./client_directory/%s", filename );


    // Sending get-request specifing the name of the file to download.
    ret = sendto( sockfd, (const char *) request, MAXLINE, MSG_CONFIRM, (const struct sockaddr *) &(servaddr),  sizeof(servaddr) ); 
    if (ret <= 0) {
        printf("Error in function : sendto (download_request). errno = %d", errno );
        return -1;
    }

    ret = recvfrom( sockfd, (char *) buffer, MAXLINE, MSG_WAITALL , ( struct sockaddr * ) &upload_serv_addr, &(upload_addr_len) );
    if (ret <= 0) {
        printf("\n Error in function : recvfrom (upload_request). errno %d", errno );
        return -1;
    }

    int     identifier;

    strtok( buffer, "/");

    identifier = atoi( buffer );

    ret = initialize_upload_instance( filetoupload, &upload_serv_addr, upload_addr_len, identifier );
    if (ret == -1) {
        printf("\n Error in function : initialize_upload_instance (upload_request). ");
        return -1;
    }

    printf("\n NEW UPLOAD INSTANCE INITIALIZED.");                                                                  fflush(stdout);


    return 0;

}




/*  CLIENT SIDE implementation of RUFT "DOWNLOAD ENVIRONMENT"  */


void * downloader( void * infos_ ){

    int                             ret,            counter = 0;

    struct file_download_infos      *infos = (struct file_download_infos *) infos_;

    char                            rcv_buffer[MAXLINE];

    infos -> finish = '0';


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

                printf("\n wnd_tmp->sequence_number=%d  sequence_number=%d", wnd_tmp -> sequence_number, sequence_number); fflush(stdout);

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

    printf("\n DOWNLOAD COMPLETE. ");                                   fflush(stdout);

    infos -> finish = '1';

    pthread_exit( NULL );


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

    printf("\n WRITER IS ENTERING THE CYCLE.");                             fflush(stdout);

    do {

        if (infos -> finish == '1') {
            printf("\n All file has been written on client's directory.");  fflush(stdout);
            break;
        }

        printf("\n WRITER IS in THE CYCLE.");                               fflush(stdout);
        /* Be ready to be awaken by SIGUSR2 occurrence. Go on pause. */
        
        sigpending(& set);

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

    pthread_exit( NULL );

}




/*  CLIENT SIDE implementation of RUFT "UPLOAD ENVIRONMENT".   */


typedef struct upload_infos_ {

    int                     identifier;

    char                    pathname[MAXLINE];

    char                    *buffer_cache;

    pthread_mutex_t         s_window_mutex;

    pthread_t               worker;

    pthread_t               ack_keeper;

    pthread_t               time_wizard;                                //Thread Identifier of this worker's Time_Wizard (who handles timeout-retransmission).

    struct sockaddr_in      *server_addr;                               //Address of the client who made the request.

    int                     sockfd;

    int                     len;                                        //Client address' size.

    char                    uploading;                                  //'0' : sleeping   |   '1' : working.

    sw_slot                 *sliding_window_slot_;                      //Circular linked list of sliding window's slots, related to this worker instance.

}               upload_infos;




void    * work ( void * infos ) {

    signal( SIGUSR1, wake_up);

    int ret;

    upload_infos        *info = ( upload_infos * ) infos;

    if ( ( info -> uploading )  == '0') {

        goto sleep;

    }

    redo:

    printf("\n WORKER RUNNING FOR UPLOAD.\n " );         fflush(stdout);

    

    free( info -> sliding_window_slot_ );

    info -> sliding_window_slot_ = get_sliding_window();

    ret = reliable_file_forward( ( info -> identifier ), ( info -> sockfd ), ( info -> server_addr ), ( info -> len ), 
                                    ( info -> buffer_cache ), ( info -> sliding_window_slot_ ), &( info -> s_window_mutex ) );
    if (ret == -1) {
        printf("Error in function : reliable_file forward.");
        goto redo;
    }

    info -> uploading = '0'; 

    sleep:

    pause();

    signal( SIGUSR1, wake_up);

    info -> uploading = '1';

    pthread_kill( ( info -> time_wizard ), SIGUSR1 );

    goto redo;


}


void    * acknowledgment_keeper( void * infos ) {

    signal( SIGUSR1, wake_up );


    int     ret,            len = sizeof( struct sockaddr_in );

    char    *id,            *seq_num;


    char    *buffer = malloc( sizeof( char ) * MAXLINE );

    upload_infos        *info = ( upload_infos * ) infos;

    sw_slot *sw_tmp;

    printf("\n ACKNOWLEDGMENT KEEPER RUNNING TO TRANSFER FILE : %s\n ", info -> pathname ); fflush(stdout);

    do {

        /*  Receive a packet (acknowledgments) from related block's socket.  */

        struct sockaddr_in     client_address;

        memset( buffer, 0, sizeof( buffer ) );

        ret = recvfrom( info -> sockfd, (char *) buffer, MAXLINE , MSG_WAITALL, ( struct sockaddr *) &client_address, &len); 
        if (ret <= 0) {
            printf("\n ACK KEEPER EXITS...");
            pthread_exit( NULL );
        }

        printf("\n ACK received : ");

        /*  Parse the packet to keep separated the identifier and sequence number fields.  */

        id = strtok( buffer, "/" );
        if (ret == -1)      Error_("Error in function sprintf (acknowledgment_demultiplexer).", 1);

        seq_num = strtok( NULL, "/" ) ;
        if (ret == -1)      Error_("Error in function sprintf (acknowledgment_demultiplexer).", 1);


        /*  Once the worker is found, find the worker's window's slot with sequence number as specified on ACK  */

        sw_tmp = ( info -> sliding_window_slot_ );

        while ( ( sw_tmp -> sequence_number ) != atoi(seq_num) ) {

            sw_tmp = ( sw_tmp -> next );

        }


        {
            pthread_mutex_lock( &( info -> s_window_mutex) );

            /*  THIS IS A CRITICAL SECTION FOR ACCESS ON THE SLIDING WINDOW (shared by ack-keeper thread and the relative worker).
                Update worker window's slot's status from SENT to ACKED. 
                If the slot is the first of the sliding window, forward a SIGUSR2 signal to worker-thread to get the window sliding on. */

            if ( ( sw_tmp -> status ) != SENT )  {
                printf("\n Error in acknowledgemnt keeper : unexpected window status = %d", sw_tmp ->status);
            }

            sw_tmp -> status = ACKED;

            printf(" %d", sw_tmp -> sequence_number );                  fflush(stdout);

            //current_timestamp( sw_tmp -> acked_timestamp );

            if ( ( sw_tmp -> is_first ) == '1' )    {
                pthread_kill( ( info -> worker ), SIGUSR2 ); 
                printf("\n SIGNAL THE WORKER TO SLIDE ON.");            fflush(stdout);
            }

            pthread_mutex_unlock( &( info -> s_window_mutex) );

        }         


    } while ( 1 );

    pthread_exit( NULL );

}


void    * time_wizard( void * infos ) {

    int                 ret;

    upload_infos        *info = ( upload_infos * ) infos;

    sw_slot             *window;

    sigset_t            set;

    sigemptyset( &set );
    sigaddset( &set, SIGUSR1 ); 
    sigprocmask( SIG_BLOCK, &set, NULL );

    beginning:          

    window = ( info -> sliding_window_slot_ );
    
    struct timespec     now = { 0, 0 };

    if ( info -> uploading == '0' ) {

        sigemptyset( &set );
        sigaddset( &set, SIGUSR1 ); 
        signal( SIGUSR1, wake_up );
        sigprocmask( SIG_UNBLOCK, &set, NULL );

        printf("\n Time Wizard going on pause." );                                  fflush(stdout);

        pause();

        printf("\n TIME WIZARD RUNNING.\n " );                                      fflush(stdout);

        sigprocmask( SIG_BLOCK, &set, NULL );

    } else {

        printf("\n TIME WIZARD RUNNING.\n " );                                      fflush(stdout);

    }

    do {

        printf(" . .");                                                             fflush(stdout);

        ret = nanosleep( &beat, NULL );
        if (ret == -1)      Error_( "Error in function : nanosleep() (time_wizard).", 1);

        current_timestamp( &now );

        for ( int i = 0; i < WINDOW_SIZE; i ++ ) {

            if ( 
                ( window != NULL ) 
            &&  ( window -> status == SENT ) ) {
                
                if ( nanodifftime( &now, &( window -> sent_timestamp ) )  >= ( window -> timeout_interval ) ) {

                    if ( retransmission( window, ( info -> sockfd ), ( info -> server_addr), ( info -> len ) ) == -1 )     Error_("Error in function: retransmission (time_wizard).", 1);

                }

            }

            if ( window == NULL ) break;

            window = ( window -> next );

        }

    } while( ( info -> uploading ) == '1' );

    printf("\n TIME WIZARD HAS COMPLETED A CYCLE ANG GOES TO SLEEP.");                           fflush(stdout);

    goto beginning;

}
 

int     initialize_upload_instance( char* pathname, struct sockaddr_in *serv_address, int len, int identifier ) {

    int             fd,             filesize,                ret;

    printf("\n INITIALIZE UPLOAD INSTANCE FOR FILE %s .", pathname );        fflush(stdout);

    upload_infos    *infos = malloc( sizeof( upload_infos ) );
    if (infos == NULL) {
        printf(" Error in function : malloc (initialize_upload_instance). errno %d", errno);
        return -1;
    }

    infos -> identifier = identifier;

    infos -> sockfd = sockfd;

    ret = sprintf( ( infos -> pathname), "%s", pathname );
    if (ret == -1) {
        printf("Error in function : sprintf (initialize_upload_instance). errno %d", errno );
        return -1;
    }

    pthread_mutex_init( &( infos -> s_window_mutex ), NULL) ;

    infos -> server_addr = serv_address;

    infos -> len = len;

    infos -> uploading = '1';

    fd = open( pathname, O_RDONLY);
    if (fd == -1) {
        printf("Error in function : open (initialize_upload_instance). errno %d", errno );
        return -1;
    }

    filesize = lseek( fd, 0, SEEK_END );

    infos -> buffer_cache = (char *) mmap( NULL, filesize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0 );
    if ( infos -> buffer_cache == NULL ) {
        printf("\n Error in function : mmap (initialize_upload_instance ). errno %d", errno );
        return -1;
    }

    printf("\n Opened session on file %s.\n File charged on buffer cache.\n", pathname);               fflush(stdout);


    ret = pthread_create( &( infos -> worker ), NULL, work, (void *) infos );
    if (ret == -1) {
        printf("\n Error in function : pthread_create (initialize_upload_instance). errno %d", errno );
        return -1;
    }

    ret = pthread_create( &( infos -> time_wizard ), NULL, time_wizard, (void *) infos );
    if (ret == -1) {
        printf("\n Error in function : pthread_create (initialize_upload_instance). errno %d", errno );
        return -1;
    }

    ret = pthread_create( &( infos -> ack_keeper ), NULL, acknowledgment_keeper, (void *) infos );
    if (ret == -1) {
        printf("\n Error in function : pthread_create (initialize_upload_instance). errno %d", errno );
        return -1;
    }


    return 0;

}