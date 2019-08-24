/*  Reliable Transfer Logic implementation  */
#pragma once
#include "header.h"
#include "time_toolbox.c"

/* RDT CONFIGURABLE PARAMETERS          */

#define LOSS_PROBABILITY    50

#define WINDOW_SIZE         11

#define PACKET_SIZE         512

#define HEADER_SIZE         25

#define ACK_SIZE            30


/* DEFINITION OF SLIDING WINDOW STATES   */

#define FREE                0

#define SENT                1

#define ACKED               2


/* DEFINITION OF RECEIVING WINDOW STATES */ 

#define WAITING             3

#define RECEIVED            4




typedef struct sliding_window_slot_ {

    int                     sequence_number;                          // Sequence number of related packet.

    int                     bytes;                                    // Actual number of packet's bytes

    char                    is_first;                                 // '0' : not first    |    '1' : first.  

    char                    retransmission;                           // Flag representing a retransmitted packet or not.  

    int                     status;                                   // FREE - SENT - ACKED.

    struct timespec         sent_timestamp;                           // Istant of packet's forwarding.

    struct timespec         acked_timestamp;                          // Istant of ack receiving.

    long                    timeout_interval;                         // Timeout Interval of retransmission.

    char                    packet[ MAXLINE + 64 ];                   // The packet.          

    struct sliding_window_slot_     *next;                            // Pointer to the next slot.

}               sw_slot;






typedef struct receiving_window_slot_ {

    char                    is_first;                                 // '0' : not first    |    '1' : first.  

    int                     sequence_number;                          // Sequence number of related packet.                             

    int                     status;                                   // WAITING - RECEIVED

    char                    *packet;                                  // Received packet.

    char                    *header;                                  // Received packet's header.

    struct receiving_window_slot_     *next;                          // Pointer to the next slot.

}               rw_slot;


/*  EVENT HANDLERS  */

void wake_up(){}                                                      // SIGUSR1 handler. 

void incoming_ack(){}                                                 // SIGUSR2 handler.






/*  
    This function allocates, initiate and returns a new sliding window of size WINDOW_SIZE, 
    as defined at the top of this file.   
*/
sw_slot*   get_sliding_window() {

    sw_slot * window;

    window = malloc( sizeof( sw_slot ) );
    if (window == NULL){
        printf("Error in function : malloc (get_sliding_window).");
        return NULL;
    }

    window -> sent_timestamp = (struct timespec) {0, 0};
    window -> is_first = '1';
    window -> sequence_number = 0;
    window -> bytes = 0;
    window -> status = FREE;

    sw_slot * tmp = window;

    for ( int i = 1 ; i < WINDOW_SIZE ; i ++ ) {

        tmp -> next = malloc( sizeof( sw_slot ) );
        if ( ( tmp -> next ) == NULL) {
            printf("Error in function : malloc (get_sliding_window).");
            return NULL;
        }

        tmp = ( tmp -> next );

        tmp -> sent_timestamp = (struct timespec) {0, 0};
        tmp -> is_first = '0';
        tmp -> sequence_number = i;
        tmp -> bytes = 0;
        tmp -> status = FREE;

    }

    tmp -> next  = window;

    return window;

}


/*  
    This function allocates, initiate and returns a new receiving window of size WINDOW_SIZE, 
    as defined at the top of this file.  
*/
rw_slot*   get_rcv_window() {

    rw_slot * window;

    window = malloc( sizeof( rw_slot ) );
    if (window == NULL){
        printf("Error in function : malloc (get_sliding_window).");
        return NULL;
    }

    window -> sequence_number = 0;
    window -> is_first = '1';
    window -> status = WAITING;

    rw_slot * tmp = window;

    for ( int i = 1 ; i < WINDOW_SIZE ; i ++ ) {

        tmp -> next = malloc( sizeof( sw_slot ) );
        if ( ( tmp -> next ) == NULL) {
            printf("Error in function : malloc (get_sliding_window).");
            return NULL;
        }

        tmp = ( tmp -> next );

        tmp -> sequence_number = i;
        tmp -> is_first = '0';
        tmp -> status = WAITING;

    }

    tmp -> next  = window;

    return window;

}




/*  
    This function implements the reliable file transfer logic. 
    This function takes as argument a File Stream containing the file to forward, specified in buffer_cache.
    The file has to be sent to the client_address through the block's socket specified in socket_descriptor.
    Reliable Data Transfer is implemented as a pipelining ' sliding window 'protocol, using ACKs and 
    timout-retransmissions.
*/
int reliable_file_forward( int identifier, int    socket_descriptor, struct sockaddr_in  *client_address, int len , 
                           char  *buffer_cache , sw_slot   *window, pthread_mutex_t   *mutex ) {

    int         ret,        filesize,       counter = 0,         endfile = 0;

    /*  Set the Sliding Window for this transfer occurrence. */

    sw_slot     *tmp =      window;

    char        *packet = malloc( sizeof(char) * MAXLINE );                            
    if (packet == NULL) {
        printf("Error in function : malloc (reliable data transfer).");
        return -1;
    }

    char        *packet_header = malloc ( sizeof(char) * HEADER_SIZE );
    if (packet_header == NULL) {
        printf("Error in function : malloc (reliable data transfer).");
        return -1;
    }

    char        *packet_content = malloc ( sizeof(char) * PACKET_SIZE );
    if (packet_content == NULL) {
        printf("Error in function : malloc (reliable data transfer).");
        return -1;
    }

    filesize =  strlen( buffer_cache );                                                 
    
    /* Temporarily block SIGUSR2 occurrences to this thread. */

    sigset_t set;
    sigemptyset( &set );
    sigaddset( &set, SIGUSR2);
    sigprocmask( SIG_BLOCK, &set, NULL);
    signal( SIGUSR2, incoming_ack);

    printf("\033[1;36m");

    printf("\n SENDING DOWNLOAD INFOS TO THE CLIENT..." ); fflush( stdout ); 

    /*  Send worker identifier and total file size to the client, so that it could 
        begin and complete this file download instance in the right way.  */

    sprintf( packet, "%d/%d/", identifier, filesize );

    printf("\n INFO PACKET : %s", packet );

    printf("\033[0m"); 

    ret = sendto( socket_descriptor, (const char *) packet, MAXLINE, MSG_CONFIRM, 
                    (const struct sockaddr *) client_address, len ); 
    if (ret == -1) {
        printf("\n Error in function sendto (reliable_file_transfer). errno = %d\n", errno );
        return -1;
    }

    do { 

        if ( endfile == filesize ) {
            printf("\n ALL PACKETS TRANSMITTED ONCE.\n WAITING FOR ALL ACKNOWLEDGEMENTS...");     
            fflush(stdout);
            goto stop;   
        }
        
        

        printf("\n DOWNLOAD IN PROGRESS...");                                 fflush(stdout);

        while ( ( tmp -> status ) == FREE && ( endfile < filesize ) ) {   

            pthread_mutex_lock( mutex );                          

            /*  Populate packet's contents.  */

            memset( packet, 0, sizeof( packet ) );

            ret = sprintf( packet_header, "%d/%d/", identifier , ( tmp -> sequence_number ) );                          
            if (ret == -1) {
                printf("Error in function: sprintf (reliable_file_transfer).");
                return -1;
            }

            /*  Write the actual header on header packet string. */
            ret = snprintf( packet_content, ( PACKET_SIZE + 1 ), "%s", buffer_cache + 
                                                            ( PACKET_SIZE * ( tmp -> sequence_number ) ) );     
            if (ret == -1) {
                printf("Error in function : sprintf (reliable data transfer).");
                return -1;
            }

            ret = sprintf( packet ,"%s", packet_header );
            if (ret == -1) {
                printf("Error in function : sprintf (reliable data transfer).");
                return -1;
            }
            ret = sprintf( packet + strlen(packet) ,"%s", packet_content );
            if (ret == -1) {
                printf("Error in function : sprintf (reliable data transfer).");
                return -1;
            }


            sprintf( ( tmp -> packet ), "%s", packet );

            tmp -> retransmission = '0';

            /*  Set the Timeout Interval for retransmission. */
            tmp -> timeout_interval = TIMEOUT_INTERVAL;                                           
            
            /*  Set packet's Sent-Timestamp. */
            current_timestamp( &( tmp -> sent_timestamp ) );                                      

            ret = sendto( socket_descriptor, (const char *) packet, MAXLINE, MSG_CONFIRM, 
                            (const struct sockaddr *) client_address, len ); 
            if (ret == -1) {
                printf("Error in function sendto (reliable_file_transfer). errno = %d ", errno );
                return -1;
            }
            printf("\033[1;32m");

            printf("\n SENT PACKET %s with content size of %ld \n", packet_header, strlen(packet_content) );    
            fflush(stdout);

            printf("\033[0m"); 
        
            /*  Set slot's bytes value with the precise number of file bytes sent. */
            tmp -> bytes += strlen( packet_content );                                             
            
            /*  Update Sliding Window's state. */
            tmp -> status = SENT;                                                                 

            tmp = ( tmp-> next );

            endfile += strlen( packet_content );  

            pthread_mutex_unlock( mutex );

            /*  FILE-IS-OVER condition : All bytes sent. */
            if ( strlen( packet_content ) < ( PACKET_SIZE ) )     break;                        

        }

        stop: 

        if( counter == filesize)        goto end;

        /*  Here thread is going on pause until the first window's slot's state is set to ACKED.
            Pause state expects a signal to let the thread being awaken. 
            Signal expected is SIGUSR2, forwarded by the ACK-Thread of the block and used to 
            notify an acknowledgment to a worker.   */
        
        signal( SIGUSR2, incoming_ack ); 
        sigpending( &set );
        if ( sigismember( &set, SIGUSR2 )  )  {
            sigprocmask( SIG_UNBLOCK, &set, NULL);
            goto action;
        }    
        
        sigemptyset( &set );
        sigaddset( &set, SIGUSR2 );
        sigprocmask( SIG_UNBLOCK, &set, NULL);

        pause();

        action:

        printf("\n SLIDING THE WINDOW ON...");               fflush(stdout);

        pthread_mutex_lock( mutex );

        /*  Temporarily block all signals to this thread.  */

        sigprocmask( SIG_BLOCK, &set, NULL);

        while( tmp -> status != ACKED && tmp -> is_first != '1' ){

            tmp = tmp ->next;
        }

        while ( ( tmp -> status ) == ACKED ) {

            tmp -> status = FREE;                                                //reset slot's status to FREE, to be reused by next packets.                                         

            counter += ( tmp -> bytes );                                         //update counter value with acknowledged bytes of this slot.

            // printf("\n tmp->bytes=%d  counter=%d", tmp -> bytes, counter );          
            // fflush(stdout);

            tmp -> bytes = 0;                                                    //reset slot's bytes to 0, to be reused by next packets.

            tmp -> is_first = '0';                                               //set the is_first flag to '0' : this slot is becoming the last one of the window.


            /*  Slide the window on. */

            tmp -> sequence_number += WINDOW_SIZE;

            tmp = ( tmp -> next );

            tmp -> is_first = '1';

            window = tmp;    
                                                              

        }

        /*  Set tmp value to the first FREE slot to start over the cycle from the top.  */

        printf(" WINDOW SLIDED ON.");                                     fflush(stdout);

        pthread_mutex_unlock( mutex );

        printf( "\n Counter = %d - filesize = %d", counter, filesize);         fflush(stdout);

 

    } while( counter < filesize );

    end:

    printf("\033[1;34m");

    printf("\n TRANSMISSION COMPLETE.");    fflush(stdout);

    printf("\033[0m"); 

    return counter;

}





/*
    This function implements packet's retransmission. 
    It takes as arguments a window, the block's socket descriptor and the client address forwarding the packet to.
    Returns 0 on success, -1 on failure.
*/
int retransmission( sw_slot *window, int socket_descriptor, struct sockaddr_in  *clientaddress , int len ) {

    int ret;

    window -> retransmission = '1';

    if( ADAPTIVE == '1')                TIMEOUT_INTERVAL = 2 * TIMEOUT_INTERVAL;

    ret = sendto( socket_descriptor, ( const char *) window -> packet , MAXLINE, 
                  MSG_CONFIRM, (const struct sockaddr *) clientaddress, len ); 
    if (ret == -1) {
        printf("Error in function sendto (reliable_file_transfer).");
        return -1;
    }

    printf("\033[01;33m");

    printf("\n RETRANSMISSION OF PACKET N° %d \n ", ( window -> sequence_number ) );    

    printf("\033[0m");         

    fflush(stdout);


    current_timestamp( &( window -> sent_timestamp ) );                            // Set packet's sent-timestamp.

    return 0; 

}






int update_adaptive_timeout_interval( sw_slot *packet ) {

    long sampleRTT = nanodifftime( &( packet -> acked_timestamp ), &( packet -> sent_timestamp ) );

    if( EstimatedRTT == 0)              EstimatedRTT = sampleRTT;

    EstimatedRTT = ( 0.875  * EstimatedRTT ) + ( 0.125 * sampleRTT );

    DevRTT = ( 0.75 * DevRTT ) + ( 0.25 * ( sampleRTT - EstimatedRTT ) );

    TIMEOUT_INTERVAL = EstimatedRTT + ( 4 * DevRTT );

    return 0;

}