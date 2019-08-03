/*  Here is implemented all of the Reliable Transfer Logic, used by transferring sections of RUFT client's and server's software. */
#pragma once
#include "header.h"
#include "time_toolbox.c"

/* RDT CONFIGURABLE PARAMETERS */

#define LOSS_PROBABILITY    6

#define WINDOW_SIZE         7

#define PACKET_SIZE         512

#define HEADER_SIZE         25

#define ACK_SIZE            30


/* DEFINE SLIDING WINDOW STATES */

#define FREE                0

#define SENT                1

#define ACKED               2


/* DEFINE RECEIVING WINDOW STATES */ 

#define WAITING             3

#define RECEIVED            4





typedef struct sliding_window_slot_ {

    int                     sequence_number;                            //Sequence number of related packet.

    int                     bytes;                                      //Actual number of packet's bytes

    char                    is_first;                                   // '0' : not first    |    '1' : first.    

    int                     status;                                     // FREE - SENT - ACKED.

    struct timespec         *sent_timestamp;                            //Istant of packet's forwarding.

    struct timespec         *acked_timestamp;                           //Istant of ack receiving.

    long                    timeout_interval;                           //Timeout Interval of retransmission.

    char                    *packet;                                    //The packet.          

    struct sliding_window_slot_     *next;                              //Pointer to the next slot.

}               sw_slot;






typedef struct receiving_window_slot_ {

    char                    is_first;                                   // '0' : not first    |    '1' : first.  

    int                     sequence_number;                            //Sequence number of related packet.                             

    int                     status;                                     // WAITING - RECEIVED

    char                    *packet;                                    //Received packet.

    char                    *header;                                    //Received packet's header.

    struct receiving_window_slot_     *next;                            //Pointer to the next slot.

}               rw_slot;






/*  This function allocates, initiate and returns a new sliding window of size WINDOW_SIZE, as defined at the top of this file.   */
sw_slot*   get_sliding_window() {

    sw_slot * window;

    window = malloc( sizeof( sw_slot ) );
    if (window == NULL){
        printf("Error in function : malloc (get_sliding_window).");
        return NULL;
    }

    window -> is_first = '1';
    window -> sent_timestamp = malloc( sizeof( struct timespec) );
    if ( window -> sent_timestamp == NULL ) {
        printf("Error in function : malloc (get sliding window).");
        return NULL;
    }
    window -> acked_timestamp = malloc( sizeof( struct timespec) );
    if ( window -> acked_timestamp == NULL ) {
        printf("Error in function : malloc (get sliding window).");
        return NULL;
    }
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

        tmp -> is_first = '0';
        window -> sent_timestamp = malloc( sizeof( struct timespec) );
        if ( window -> sent_timestamp == NULL ) {
            printf("Error in function : malloc (get sliding window).");
            return NULL;
        }
        window -> acked_timestamp = malloc( sizeof( struct timespec) );
        if ( window -> acked_timestamp == NULL ) {
            printf("Error in function : malloc (get sliding window).");
            return NULL;
        }
        tmp -> sequence_number = i;
        tmp -> bytes = 0;
        tmp -> status = FREE;

    }

    tmp -> next  = window;

    return window;

}

/*  This function allocates, initiate and returns a new receiving window of size WINDOW_SIZE, as defined at the top of this file.   */
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



/*  EVENT HANDLERS DECLARATION : SIGUSR1 & SIGUSR2 */

void wake_up(){}                                                                                                   //SIGUSR1 handler. 

void incoming_ack(){}                                                                                              //SIGUSR2 handler.




/*  
    This function implements the reliable file transfer logic. 
    This function takes as argument a File Stream containing the file to forward, specified in buffer_cache.
    The file has to be sent to the client_address through the block's socket specified in socket_descriptor.
    Reliable Data Transfer is implemented as a pipelining ' sliding window 'protocol, using ACKs and timout-retransmissions.
*/
int reliable_file_forward( int identifier, int    socket_descriptor, struct sockaddr_in  *client_address, int len , 
                           char  *buffer_cache , sw_slot   *window, pthread_mutex_t   *mutex ) {

    int         ret,        filesize,       counter = 0,         endfile = 0;

    /*  Get the sliding window for this transfer occurrence. */

    sw_slot     *tmp =                  window;

    sw_slot     *first_free =           window;


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

    filesize =  strlen( buffer_cache );                                                 //variable used to check if all file has been sent after transaction.
    

    /* Temporarily block all signals to this thread. */

    sigset_t set;
    sigemptyset( &set );
    sigaddset( &set, SIGUSR2);
    sigprocmask( SIG_BLOCK, &set, NULL);
    signal( SIGUSR2, incoming_ack);

    printf("\n Sending Download informations to the client..." ); fflush( stdout ); 

    /*  Send worker identifier and total file size to the client, so that it could actually start (and finish!) this file download instance.  */
    sprintf( packet, "%d/%d/", identifier, filesize );

    printf("\n INFO PCKT : %s", packet );

    ret = sendto( socket_descriptor, (const char *) packet, MAXLINE, MSG_CONFIRM, (const struct sockaddr *) client_address, len ); 
    if (ret == -1) {
        printf("\n Error in function sendto (reliable_file_transfer). errno = %d\n", errno );
        return -1;
    }

    do { 

        if ( endfile == filesize )   goto stop;
        
        //pthread_mutex_lock( mutex );

        printf("\n DOWNLOAD IN PROGRESS...");       fflush(stdout);

        while ( ( tmp -> status ) == FREE && ( endfile < filesize ) ) {                             
            
            sleep(1);

            /*  Populate packet's contents ( worker identifier, sequence number and related chunck of file bytes). */

            memset( packet, 0, sizeof( packet ) );

            ret = sprintf( packet_header, "%d/%d/", identifier , ( tmp -> sequence_number ) );                          
            if (ret == -1) {
                printf("Error in function: sprintf (reliable_file_transfer).");
                return -1;
            }

            ret = snprintf( packet_content, ( PACKET_SIZE + 1 ), "%s", buffer_cache + ( PACKET_SIZE * ( tmp -> sequence_number ) ) );                                            //write the actual header on header packet string.
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


            tmp -> packet = malloc( sizeof(char) * sizeof(packet) );                              //temporarily write the packet on its sliding window's slot.
            if ( tmp -> packet == NULL ) {
                printf(" Error in function : malloc (reliable file forward).");
                return -1;
            }
            sprintf( tmp -> packet, "%s", packet );


            tmp -> timeout_interval = TIMEOUT_INTERVAL;                                           //set the timeout interval for retransmission.


            ret = sendto( socket_descriptor, (const char *) packet, MAXLINE, MSG_CONFIRM, 
                            (const struct sockaddr *) client_address, len ); 
            if (ret == -1) {
                printf("Error in function sendto (reliable_file_transfer). errno = %d ", errno );
                return -1;
            }

            printf("\n SENT PACKET %s with content size of %ld \n", packet_header, strlen(packet_content) );    fflush(stdout);
            
            

            //current_timestamp( tmp -> sent_timestamp );                                         //set packet's sent-timestamp.

            tmp -> bytes += strlen( packet_content );                                             //set slot's bytes value with the precise number of file bytes sent.
            
            tmp -> status = SENT;                                                                 //update sliding window's state.

            tmp = ( tmp-> next );

            endfile += strlen( packet_content );  

            if ( strlen( packet_content ) < ( PACKET_SIZE ) )     break;                        //file-is-over condition : all bytes sent.

        }

        //pthread_mutex_unlock( mutex );

        stop: 

        if( counter == filesize)        goto end;

        /*  Here thread is going on pause until the first window's slot's state is set to ACKED.
            Pause state expects a signal to let the thread being awaken. 
            Signal expected is SIGUSR2, forwarded by the ACK-Thread of the block and used to 
            notify an acknowledgment to a worker.   */

        printf("\n WORKER IS GOING ON PAUSE...");                           fflush(stdout);
        
        signal( SIGUSR2, incoming_ack ); 
        sigpending( &set );
        if ( sigismember( &set, SIGUSR2 )  )  {
            sigprocmask( SIG_UNBLOCK, &set, NULL);
            printf("\n SIGUSR2 pending on mask! goto action.");             fflush(stdout);
            goto action;
        }    
        
        sigemptyset( &set );
        sigaddset( &set, SIGUSR2 );
        sigprocmask( SIG_UNBLOCK, &set, NULL);

        pause();

        action:

        printf("\n WORKER AWAKEN, SLIDING THE WINDOW ON...");               fflush(stdout);

        //pthread_mutex_lock( mutex );

        /*  Temporarily block all signals to this thread.  */

        sigprocmask( SIG_BLOCK, &set, NULL);


        first_free = tmp;

        while(tmp->status != ACKED && tmp->is_first != '1'){
            tmp = tmp ->next;
        }

        while ( ( tmp -> status ) == ACKED ) {

            tmp -> status = FREE;                                                //reset slot's status to FREE, to be reused by next packets.                                         

            counter += ( tmp -> bytes );                                         //update counter value with acknowledged bytes of this slot.

            printf("\n tmp->bytes=%d  counter=%d", tmp->bytes,counter);          fflush(stdout);

            tmp -> bytes = 0;                                                    //reset slot's bytes to 0, to be reused by next packets.

            tmp -> is_first = '0';                                               //set the is_first flag to '0' : this slot is becoming the last one of the window.


            /*  Slide the window on. */

            tmp -> sequence_number += WINDOW_SIZE;
            tmp = ( tmp -> next );
            tmp -> is_first = '1';
            window = tmp;    
                                                              

        }

        /*  Set tmp value to the first FREE slot to start over the cycle from the top.  */

        tmp = first_free;

        printf("\n WINDOW SLIDED ON.");                                     fflush(stdout);

        //pthread_mutex_trylock( mutex );

        //pthread_mutex_unlock( mutex );

        printf( "counter = %d - filesize = %d", counter, filesize);         fflush(stdout);

 

    } while( counter < filesize );

    end:

    printf("\n TRANSMISSION COMPLETE.");    fflush(stdout);

    return counter;


}


int reliable_file_receive();





/*
    This function implements packet's retransmission. 
    It takes as arguments a window, the block's socket descriptor and the client address forwarding the packet to.
    Returns 0 on success, -1 on failure.
*/
int retransmission( sw_slot *window, int socket_descriptor, struct sockaddr_in  *clientaddress ) {

    int ret;

    ret = sendto( socket_descriptor, (const char *) window -> packet , strlen( window -> packet ), 
                  MSG_CONFIRM, (const struct sockaddr *) clientaddress, sizeof( *clientaddress ) ); 
    if (ret == -1) {
        printf("Error in function sendto (reliable_file_transfer).");
        return -1;
    }

    current_timestamp( window -> sent_timestamp );                                               //set packet's sent-timestamp.

    return 0; 

}