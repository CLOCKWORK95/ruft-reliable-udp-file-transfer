/* Client and Server side implementation of RUFT : UPLOAD ENVIRONMENT */
#pragma once
#include "header.h"
#include "Reliable_Data_Transfer.c"
#include "time_toolbox.c"

#define POOLSIZE   10


/*  SERVER SIDE implementation of RUFT Upload Environment. 
    The following structures are used to receive data from clients uploading files on server's directory.  */

struct packet {

    char                    data[PACKET_SIZE];

    struct packet           *next;

};


struct upload_block {

    char                    filepath[MAXLINE];

    struct sockaddr_in      clientaddr;

    int                     addr_len;

    struct packet           *packet_queue;

    pthread_t               uploader;

    pthread_t               writer;

    rw_slot                 *rcv_wnd;

};


struct sockaddr_in          upload_socket;          int         upload_socket_len;

struct upload_block         *upload_environment;







/*  CLIENT SIDE implementation of RUFT Upload Environment. 
    The following structures are used to receive data from clients uploading files on server's directory.  */


typedef struct upload_infos_ {

    int                     identifier;

    char                    pathname[MAXLINE];

    char                    *buffer_cache;

    pthread_mutex_t         s_window_mutex;

    pthread_t               worker;

    pthread_t               ack_keeper;

    pthread_t               time_wizard;                                //Thread Identifier of this worker's Time_Wizard (who handles timeout-retransmission).

    struct sockaddr_in      *server_addr;                               //Address of the client who made the request.

    int                     len;                                        //Client address' size.

    int                     sockfd;                                     //Socket descriptor through which sending packets.

    char                    uploading;                                 //'0' : sleeping   |   '1' : working.

    sw_slot                 *sliding_window_slot_;                      //Circular linked list of sliding window's slots, related to this worker instance.

}               upload_infos;





void * work ( void * infos ) {

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


void * acknowledgment_keeper( void * infos ) {

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



void * time_wizard( void * infos ) {

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
 
