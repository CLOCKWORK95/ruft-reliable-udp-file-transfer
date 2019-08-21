/* Client and Server side implementation of RUFT : UPLOAD ENVIRONMENT */
#pragma once
#include "header.h"
#include "Reliable_Data_Transfer.c"
#include "time_toolbox.c"

#define POOLSIZE   10

pthread_mutex_t rcv_window_mutex = PTHREAD_MUTEX_INITIALIZER;


/*  SERVER SIDE implementation of RUFT Upload Environment. 
    The following structures are used to receive data from clients uploading files on server's directory.  */


struct upload_block {

    int                     sockfd;                             //Socket descriptor for this upload block.

    int                     identifier;                         //Identifier code for this upload block.

    char                    filepath[MAXLINE];                  //Current file pathname ( variable for each upload instance).

    char                    ACK[ACK_SIZE]; 

    struct sockaddr_in      *clientaddr;                        //Current client's address.

    int                     addr_len;                           //Current client address' lenght.

    pthread_t               uploader;

    pthread_t               writer;

    rw_slot                 *rcv_wnd;

    char                    uploading;

    struct upload_block     *next;

};                                                                  struct upload_block         *upload_environment;



void    * uploader( void * upload_block ) {

    int                             ret,            counter = 0;

    struct upload_block             *block = (struct upload_block *) upload_block;

    char                            rcv_buffer[MAXLINE];

    start: 

    if ( block -> uploading == '0') {

        signal( SIGUSR1, wake_up );

        pause();

    }


    // Upload File.

    printf("\n PREPEARING TO UPLOAD...\n Getting informations about the uploading file...");            fflush(stdout);

    block -> rcv_wnd = get_rcv_window();

    block -> addr_len = sizeof( struct sockaddr_in );

    /* Receive the file size and the identifier of server worker matched to this download instance. */
    ret = recvfrom( block -> sockfd, (char *) rcv_buffer, MAXLINE,  MSG_WAITALL, (struct sockaddr *) &( block -> clientaddr ), &( block -> addr_len ) ); 
    if (ret <= 0)       Error_("Error in function : recvfrom (downloader).", 1);

    printf("\n RECEIVED infos : %s", rcv_buffer );                                                      fflush(stdout); 

    /* Initiate the exit-condition's values for the next cycle. */
    char *idtf;
    idtf =                strtok( rcv_buffer, "/" );
    block -> identifier = atoi( idtf );

    printf("\n - WORKER ID : %d", block -> identifier );                                                fflush(stdout); 

    char *filesz;
    filesz =              strtok( NULL, "/" );
    int filesize =        atoi( filesz );

    printf("\n - SIZE : %d", filesize );                                                                fflush(stdout); 

    memset( rcv_buffer, 0, MAXLINE);

    do{

        printf("\n  UPLOAD IN PROGRESS... ");                                                         fflush(stdout);

        ret = recvfrom( block -> sockfd, (char *) rcv_buffer, MAXLINE,  MSG_WAITALL, 
                        (struct sockaddr *) &( block -> clientaddr ), &( block -> addr_len ) ); 
        if (ret <= 0)       Error_("Error in function : recvfrom (uploader).", 1);

        char    *idtf;                          idtf = strtok( rcv_buffer, "/");
        int     identifier = atoi( idtf );

        
        if ( identifier == ( block -> identifier ) ) {

            /*  THIS IS A CRITIAL SECTION FOR RECEIVING WINDOWS ACCESS ON WRITING. 
                DOWNLOADER THREAD TAKES A TOKEN FROM MUTEX TO RUN THIS CODE BLOCK. */

            if ( pthread_mutex_lock( &rcv_window_mutex ) == -1 )        Error_("Error in function : pthread_mutex_lock (uploader).", 1);

            rw_slot     *wnd_tmp = ( block -> rcv_wnd );

            char        *sn = strtok( NULL, "/" );

            int         sequence_number = atoi( sn );

            printf("\n --> Arrived packet with sequence number : %d .", sequence_number);               fflush(stdout);

            for (int i = 0; i < WINDOW_SIZE; i++) {

                printf("\n wnd_tmp->sequence_number=%d  sequence_number=%d", wnd_tmp -> sequence_number, sequence_number); fflush(stdout);

                if ( wnd_tmp -> sequence_number == sequence_number ) {

                    /* Send an ACKNOWLEDGMENT to the RUFT Server Side. */

                    sprintf( ( block -> ACK ), "%d/%d/", identifier, sequence_number );

                    printf(" SENDING ACK : %s", block -> ACK);

                    ret = sendto( block -> sockfd, (const char *) ( block -> ACK ), MAXLINE, MSG_CONFIRM, 
                                 (const struct sockaddr *) &( block -> clientaddr ), block -> addr_len ); 
                    if (ret <= 0) {
                        printf("\n Error in function : sendto (uploader). errno = %d ", errno );
                        exit(-1);
                    }

                    /* Update rcv_window's slot status.  */
                    wnd_tmp -> status = RECEIVED;

                    wnd_tmp -> packet = malloc( sizeof(char) * MAXLINE );
                    if (wnd_tmp -> packet == NULL)      Error_( "Error in function : malloc (downloader).", 1);
                    if ( sprintf( ( wnd_tmp -> packet ), "%s", ( rcv_buffer + ( strlen(idtf) + strlen(sn) + 2 ) ) ) == -1 )        
                                                                         Error_( "Error in function : sprintf (uploader).", 1);

                    counter += strlen( wnd_tmp -> packet);

                    if ( ( wnd_tmp -> is_first ) == '1' ) {

                        printf( "\n SENDING SIGNAL TO WRITER FOR PACKET %d", sequence_number );      fflush(stdout); 

                        /* If this is the first slot of the window, then alert the writer about it (SIGUSR2) so that it could slide the rcv_window on. */

                        pthread_kill( block -> writer, SIGUSR2 );

                        if ( pthread_mutex_unlock( &rcv_window_mutex ) == -1 )        Error_("Error in function : pthread_mutex_unlock (uploader).", 1);

                    }

                    break;
                }

                wnd_tmp = wnd_tmp -> next;

            }

            if ( pthread_mutex_unlock( &rcv_window_mutex ) == -1 )        Error_("Error in function : pthread_mutex_unlock (uploader).", 1);


            /* END OF CRITICAL SECTION FOR RECEIVING WINDOW'S ACCESS. */

        }

        printf("\n counter = %d, filesize = %d", counter, filesize);        fflush(stdout);

        memset( rcv_buffer, 0, strlen(rcv_buffer) );

    } while( counter < filesize );

    printf("\n UPLOAD COMPLETE.");                                          fflush(stdout);

    block -> uploading = '0';

    free( block -> rcv_wnd);

    goto start;

}


void    * writer( void * upload_block ) {

    struct upload_block             *block = (struct upload_block *) upload_block;

    sigset_t    set;

    start:

    if ( block -> uploading == '0') {

        signal( SIGUSR2, wake_up );

        pause();

    }


    /* Temporarily block SIGUSR2 signal occurrences. */
    sigemptyset( &set );
    sigaddset( &set, SIGUSR2);
    sigprocmask( SIG_BLOCK, &set, NULL );

    int                                         ret,            file_descriptor,            counter = 0;

    /* Create the new file in client's directory or truncate an existing one with the same pathname, to start download. */
    file_descriptor = open( ( block -> filepath ), O_RDWR | O_CREAT , 0660 );  
    if (file_descriptor == -1) {
        printf("\n Error in function : open (writer). errno = %d", errno);
        pthread_exit(NULL);
    }

    printf("\n WRITER IS ENTERING THE CYCLE.");                             fflush(stdout);

    do {
        
        if ( block -> uploading == '0') {
            printf("\n All file has been written on server's directory.");  fflush(stdout);
            break;
        }

        printf("\n WRITER IS in THE CYCLE.");                               fflush(stdout);
        /* Be ready to be awaken by SIGUSR2 occurrence. Go on pause. */
        
        sigpending(& set);

        if ( sigismember( &set, SIGUSR2 ) ) {
            signal( SIGUSR2, wake_up );
            sigprocmask( SIG_UNBLOCK, &set, NULL );
            printf("\n SIGUSR2 pending on mask! goto action.");             fflush(stdout);
            goto action;
        }

        signal( SIGUSR2, wake_up );
        sigemptyset( &set );
        sigaddset( &set, SIGUSR2);
        sigprocmask( SIG_UNBLOCK, &set, NULL );

        printf("\n SIGUSR2 UNBLOCKED");                                     fflush(stdout);

        pause();

        action:
        
        printf("\n SIGUSR2 BLOCKED");                                       fflush(stdout);

        /* Temporarily block SIGUSR2 signal occurrences. */
        sigprocmask( SIG_BLOCK, &set, NULL );

        printf( "\n WRITER AWAKED" );                                      fflush(stdout);

        {   
            /*  THIS IS A CRITIAL SECTION FOR RECEIVING WINDOWS ACCESS ON WRITING. 
                WRITER THREAD TAKES A TOKEN FROM MUTEX TO RUN THIS CODE BLOCK. */

            if ( pthread_mutex_lock( &rcv_window_mutex ) == -1 )        Error_("Error in function : pthread_mutex_lock (writer).", 1);
        
            rw_slot      *wnd_tmp = ( block -> rcv_wnd );

            rw_slot      *curr_first   = ( block -> rcv_wnd );

            while( ( curr_first -> status != RECEIVED ) && ( curr_first -> is_first  != '1') ) {
                curr_first = ( curr_first -> next );
            }

            while ( curr_first -> status == RECEIVED ) {

                lseek( file_descriptor, 0, SEEK_END );

                /* Write the packet within the new file in client's directory. */
                ret = write( file_descriptor, ( curr_first -> packet ), strlen( curr_first -> packet  ) );
                if ( ret == -1)         Error_( "Error in function : write (thread writer).", 1);

                printf( "\n Packet %d content has been written on file %s. %d bytes written .", 
                                            ( curr_first -> sequence_number ), ( block -> filepath ), ret );            fflush(stdout);


                /* Slide the receiving window on. */

                ( curr_first -> sequence_number ) += WINDOW_SIZE;
                ( curr_first -> status ) = WAITING;
                memset( ( curr_first -> packet ), 0, sizeof( curr_first -> packet ) );
                curr_first -> is_first = '0';

                curr_first -> next -> is_first = '1';
                curr_first = ( curr_first -> next );
                block -> rcv_wnd = curr_first;

                printf("\n WINDOW SLIDED ON");      fflush(stdout);
                
            
            }


            if ( pthread_mutex_unlock( &rcv_window_mutex ) == -1 )        Error_("Error in function : pthread_mutex_unlock (writer).", 1);

            /* END OF THE CRITICAL SECTION. */

        }



    } while (1);

    block -> uploading = '0';

    sigemptyset( &set );
    sigaddset( &set, SIGUSR2 );
    sigprocmask( SIG_UNBLOCK, &set, NULL );

    goto start;


}




int     initialize_upload_environment() {

    int                     ret;

    upload_environment = malloc( sizeof( struct upload_block ) );
    if (upload_environment == NULL ) {
        printf("Error in function : malloc (init_upload_environment). errno %d", errno );
        exit(1);
    }

    struct upload_block     *tmp;

    tmp = upload_environment;

    for ( int i = 0; i < POOLSIZE; i++ ) {

        if ( ( ( tmp -> sockfd ) = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {            //creating block's socket file descriptor 
            perror("\n socket creation failed (init new block)."); 
            return -1; 
        } 

        tmp -> identifier = i;

        tmp -> uploading = '0';

        ret = pthread_create( &( tmp -> uploader ), NULL, uploader, ( void * ) tmp );
        if (ret == -1) {
            printf("Error in function : pthread_create (initialize_upload_environment). errno %d", errno );
            exit(1);
        }

        ret = pthread_create( &( tmp -> writer ), NULL, writer, (void *) tmp );
        if (ret == -1) {
            printf("Error in function : pthread_create (initialize_upload_environment). errno %d", errno );
            exit(1);
        }

        tmp -> next = malloc( sizeof( struct upload_block ) );

        tmp = tmp -> next;

    }

    return 0;

}



int     start_upload( char * filepath, struct sockaddr_in *clientaddress, int len ) {

    int                     ret;
    
    struct upload_block     *tmp;

    char                    buffer[MAXLINE];

    redo:                   tmp = upload_environment;

    while( ( tmp -> uploading ) != '0') {

        if (tmp -> next == NULL ) goto redo;

        tmp = ( tmp -> next );

    }

    sprintf( tmp -> filepath, "%s", filepath );

    tmp -> clientaddr = clientaddress;

    tmp -> addr_len = len;

    sprintf( buffer, "%d/", ( tmp -> identifier ) );

    printf("\n Buffer content : %s", buffer); fflush(stdout);

    ret = sendto( ( tmp -> sockfd ), (char *) buffer, MAXLINE, MSG_CONFIRM, (struct sockaddr *) ( tmp -> clientaddr ), ( tmp -> addr_len ) ); 
    if (ret <= 0) {
        printf("\n Error in function : sendto (start_upload). errno %d", errno );
        return -1;
    }

    tmp -> uploading = '1'; 

    pthread_kill( ( tmp -> uploader ), SIGUSR1 );

    pthread_kill( ( tmp -> writer ), SIGUSR2 );


    return 0;

}