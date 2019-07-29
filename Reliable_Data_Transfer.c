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
    window -> sequence_number = 0;
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
        window -> is_first = '0';
        tmp -> status = WAITING;

    }

    tmp -> next  = window;

    return window;

}






/*  
    This function implements the reliable file transfer logic. 
    This function takes as argument a File Stream containing the file to forward, specified in buffer_cache.
    The file has to be sent to the client_address through the block's socket specified in socket_descriptor.
    Reliable Data Transfer is implemented as a pipelining ' sliding window 'protocol, using ACKs and timout-retransmissions.
*/
int reliable_file_forward( int identifier, int    socket_descriptor, struct sockaddr_in  client_address , FILE  *buffer_cache , sw_slot   *window) {

    int         ret,    len,    filesize,   counter = 0;

    /*  Get the sliding window for this transfer occurrence. */

    window = get_sliding_window();                                                      //returns a pointer to a linked circular list of sliding window's slots.

    sw_slot     *tmp =                  window;

    sw_slot     *first_free =           window;


    char        *packet = malloc( HEADER_SIZE + PACKET_SIZE );                            
    if (packet == NULL) {
        printf("Error in function : malloc (reliable data transfer).");
        return -1;
    }

    char        *packet_header = malloc ( HEADER_SIZE );
    if (packet_header == NULL) {
        printf("Error in function : malloc (reliable data transfer).");
        return -1;
    }

    filesize =  strlen( (char *) buffer_cache );                                         //variable used to check if all file has been sent after transaction.

    
    /* Temporarily block all signals to this thread. */

    sigset_t set;
    sigfillset( &set );
    sigprocmask( SIG_BLOCK, &set, NULL);

    /*  Send worker identifier and total file size to the client, so that it could actually start (and finish!) this file download instance.  */
    sprintf( packet, "%d/%d/", identifier, filesize );
    ret = sendto( socket_descriptor, (const char *) packet, strlen(packet), MSG_CONFIRM, (const struct sockaddr *) &client_address, sizeof( client_address ) ); 
    if (ret == -1) {
        printf("Error in function sendto (reliable_file_transfer).");
        return -1;
    }
    

    do {


        while ( ( tmp -> status ) == FREE ) {

            /*  Populate packet's contents ( worker identifier, sequence number and related chunck of file bytes). */

            memset( packet, 0, sizeof( packet ) );

            ret = sprintf( packet, "%d/", identifier );                          
            if (ret == -1) {
                printf("Error in function: sprintf (reliable_file_transfer).");
                return -1;
            }

            ret = sprintf( ( packet + strlen( packet ) ), "%d/", ( tmp -> sequence_number ) );
            if (ret == -1) {
                printf("Error in function: sprintf (reliable_file_transfer).");
                return -1;
            }

            ret = sprintf( packet_header, "%s", packet );                                            //write the actual header on header packet string.
            if (ret == -1) {
                printf("Error in function : sprintf (reliable data transfer).");
                return -1;
            }

            ret = snprintf( ( packet + strlen( packet ) ), PACKET_SIZE, "%s", (char *) buffer_cache + ( PACKET_SIZE * ( tmp -> sequence_number ) ) );
            if (ret == -1) {
                printf("Error in function : snprintf (reliable data transfer).");
                return -1;
            }


            tmp -> packet = packet;                                                                 //temporarily write the packet on its sliding window's slot.

            tmp -> timeout_interval = TIMEOUT_INTERVAL;                                             //set the timeout interval for retransmission.


            /*  Send the packet to the client through the block's socket */

            ret = sendto( socket_descriptor, (const char *) packet, strlen(packet), MSG_CONFIRM, (const struct sockaddr *) &client_address, sizeof( client_address ) ); 
            if (ret == -1) {
                printf("Error in function sendto (reliable_file_transfer).");
                return -1;
            }

            current_timestamp( tmp -> sent_timestamp );                                               //set packet's sent-timestamp.

            tmp -> bytes += strlen( packet + strlen( packet_header ) );                               //set slot's bytes value with the precise number of file bytes sent.
            
            tmp -> status = SENT;                                                                     //update sliding window's state.

            tmp = ( tmp-> next );

        }

        
        /*  Here thread is going on pause until the first window's slot's state is set to ACKED.
            Pause state expects a signal to let the thread being awaken. 
            Signal expected is SIGUSR2, forwarded by the ACK-Thread of the block and used to 
            notify an acknowledgment to a worker.   */
        
        sigemptyset( &set );
        sigaddset( &set, SIGUSR2);
        sigprocmask( SIG_UNBLOCK, &set, NULL);
        
        pause();

        /*  Temporarily block all signals to this thread.  */

        sigset_t set;
        sigfillset( &set );
        sigprocmask( SIG_BLOCK, &set, NULL);


        first_free = tmp;

        while ( ( tmp -> status ) == ACKED ) {

            tmp -> status = FREE;                                                //reset slot's status to FREE, to be reused by next packets.                                         

            counter += ( tmp -> bytes );                                         //update counter value with acknowledged bytes of this slot.

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

 

    } while( counter < filesize );


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
                  MSG_CONFIRM, (const struct sockaddr *) clientaddress, sizeof( clientaddress ) ); 
    if (ret == -1) {
        printf("Error in function sendto (reliable_file_transfer).");
        return -1;
    }

    current_timestamp( window -> sent_timestamp );                                               //set packet's sent-timestamp.

    return 0; 

}