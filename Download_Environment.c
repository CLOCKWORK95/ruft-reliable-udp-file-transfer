/* Server side implementation of RUFT : DOWNLOAD ENVIRONMENT */
#pragma once
#include "header.h"
#include "Reliable_Data_Transfer.c"
#include "time_toolbox.c"

#define MAX_WORKERS     5


struct block_;


/*  
    This structure represents an istance of working thread related to a specific block of the RUFT Server's Download Environment. 
    Each of the block's workers serves download requests through the same block's socket, but for a different client. 
    As a worker thread serves a request, it goes on pause, waiting for a signal (SIGUSR1) to be awaked. 
*/
typedef struct worker_{

    pthread_mutex_t         s_window_mutex;

    pthread_t               time_wizard;                                //Thread Identifier of this worker's Time_Wizard (who handles timeout-retransmission).

    int                     identifier;                                 //Unique identifier of the worker of a block, to receive ACKs.

    struct block_           *my_block;                                  //The block containing this worker instance.

    pthread_t               tid;                                        //Identifier of the working-thread.

    struct sockaddr_in      *client_addr;                               //Address of the client who made the request.

    int                     len;                                        //Client address' size.

    int                     sockfd;                                     //Socket descriptor through which sending packets.

    char                    is_working;                                 //'0' : sleeping   |   '1' : working.

    sw_slot                 *sliding_window_slot_;                      //Circular linked list of sliding window's slots, related to this worker instance.

    struct worker_          *next;                                      //Pointer to the next worker of the same block (NULL if this is the last worker).

}               worker;




/*  
    This structure represents the first sub-division of the RUFT Server's Download Environment.
    The Download Environment itself is an array of block structs. Each block is related to a specific file on Server's persistance. 
    As a new block is allocated, a new socket is created, together with a pool of threads working exclusively for that block (workers).
    Each block refers to a file stream (buffer cache) to make workers trasmit packets fast.
    Each block has a life timer countdown, that starts running as all workers are on pause. if timer runs out, the block instance is deallocated.
*/
typedef struct block_ {

    char                    *filename;                                  //Name of the file associated to this block.

    char                    *buffer_cache;                              //Buffer cache containing the file to transmit.

    int                     server_sock_desc;                           //Block's (new) socket descriptor.

    worker                  *workers;                                   //Array of block's workers. 

    pthread_t               ack_keeper;                                 //Acknowledgments keeper and demultiplexer thread's TID.

    struct block_           *next;                                      //Pointer to next block structure.

    int                     BLTC;                                       //Block Life Timer Countdown.


}               block;                                                  block        *download_environment;



        /*  THREADS' FUNCTIONS DECLARATION */

void * work( void* _worker );


void * acknowledgment_keeper( void * _block );


void * time_wizard( void * _worker);




/*  
    This function returns a new Download Environment's block at the address specified in *new_block.
    In addition to many attributes, the generated block contains a stream of file specified in pathname, a pool of threads 
    and a new operating socket. This function activates a first thread, serving the request the function has been called to.  
    Returns : 0 on success, -1 on Error.
*/
int init_new_block ( block *new_block, char * pathname , struct sockaddr_in *client_address , int len ){ 

    printf(" :: ALLOCATE NEW BLOCK FUNCTION :: \n");

    int     ret,    fd,     filesize;    

    /* Validate memory area to contain a new block structure. */
    new_block = malloc( sizeof( block ) );
    if ( new_block == NULL ) {
        printf(" \n Error in function : malloc");
        return -1;
    }


    /* Open a new session on file pathname, and load the file in main memory to be used by this new block. */

    fd = open( pathname, O_RDONLY );

    if ( fd == -1 ) {
        printf(" \n Error in function : open (init_new block) errno = %d.", errno );
        return -1;
    }

    filesize = lseek( fd, 0, SEEK_END );

    new_block -> buffer_cache = (char *) mmap( NULL , filesize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0 );
    if ( new_block -> buffer_cache == NULL ) {
        printf(" \n Error in function : mmap (init_new block).");
        return -1;
    }

    printf("\n Opened session on file %s.\n File charged on block's cache.\n", pathname); fflush(stdout);

    /*  Populate the block structure attributes. */

    new_block -> filename = malloc( sizeof(char) * ( strlen( pathname ) + 2 ) );
    if (new_block -> filename == NULL) {
        printf(" \n Error in function : malloc (init_new block).");
        return -1;
    }
    
    ret = sprintf( (char *) ( new_block -> filename ), "%s", pathname );                                //match a file path.
    if (ret == -1) {
        printf("\n Error in function : sprintf.");
        return -1;
    }

    printf("\n Block's fields population.\n Filename : %s.\n ", new_block -> filename ); fflush(stdout);

    new_block -> BLTC = (int) ( strlen( new_block -> buffer_cache ) / PACKET_SIZE  ) ;      //set the BLTC default value proportional to the file size.

    printf("\n BLTC init value : %d.\n ", new_block -> BLTC ); fflush(stdout);

    
    if ( ( ( new_block -> server_sock_desc ) = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {                //creating block's socket file descriptor 
        perror("\n socket creation failed (init new block)."); 
        return -1; 
    }   

    printf("\n Socket N° : %d.\n ", new_block -> server_sock_desc ); fflush(stdout);

    new_block -> workers = malloc( sizeof( worker ) );                                                   //validate memory area for block's workers.
    if ( new_block -> workers == NULL ) {
        printf("Error in function : malloc.");
        return -1;
    }

    worker *tmp = new_block -> workers;                                                     

    for ( int i = 0; i < MAX_WORKERS; i ++ ) {                                                            //validate memory area for all workers.

        //populate worker's attributes.

        pthread_mutex_init( &(tmp -> s_window_mutex), NULL );

        tmp -> identifier = i;

        tmp -> is_working = '0';

        tmp -> sockfd = ( new_block -> server_sock_desc );

        tmp -> my_block = new_block;

        tmp -> sliding_window_slot_ = get_sliding_window();

        tmp -> next = malloc( sizeof( worker ) );
        if (tmp -> next == NULL) {
            printf("Error in function : malloc ( init new block).");
            return -1;
        }

        if ( i != MAX_WORKERS -1)   tmp = ( tmp -> next );
        else                        tmp -> next = ( new_block -> workers );                 //this makes a circular linked list of workers.
        
    }

    printf("\n Initialized workers' structures.\n Set up the first worker to download file. "); fflush(stdout);


    /*  Generate the thread-pool and activate the new thread to serve the request.  */

    tmp = ( new_block -> workers );

    tmp -> is_working = '1';

    tmp -> client_addr = client_address;

    tmp -> len = len;


    /*  Create the Acknowledgment Keeper Thread, to handle acknowledgments throughout the block. */

    ret = pthread_create( &( new_block -> ack_keeper ), NULL, acknowledgment_keeper, (void *) new_block );
    if (ret ==-1) {
        printf("Error in function : pthread_create(init_new_block).");
        return -1;
    }


    /*  Create and launch all concurrent threads of the pool. */

    for ( int i = 0; i < MAX_WORKERS; i ++ ) {

        ret = pthread_create( &( tmp -> time_wizard ), NULL, time_wizard, (void *) tmp );
        if ( ret == -1 ) {
            printf("Error in function: pthread_create (init_new_block).");
            return -1;
        }

        ret = pthread_create( &( tmp -> tid ), NULL, work, (void *) tmp );
        if ( ret == -1 ) {
            printf("Error in function: pthread_create (init_new_block).");
            return -1;
        }

        tmp = ( tmp -> next );
    }


    return 0;

}







/*  
    This function is tipically called by the RECEPTION ENVIRONMENT of RUFT Server.
    By this function, server reception delegates download environment sending to client_address the file specified in pathname.
    Download Environment's blocks are checked to find the one matched to that file, if already exists: 
    in that case, a paused thread of the block is awaken by SIGUSR1 event ( function pthread_kill ).
    If a block matched to pathname doesn't exists yet in Download Environment, this function calls "init_new_block" to allocate a new one.
    Returns : 0 on success, -1 on error.
*/
int start_download( char *pathname , struct sockaddr_in *client_address, int len ) {

    int                 ret;

    block               *tmp_block = download_environment;        


    /*  Verify if this is the first time the download environment is being set up.
        If it is, it is necessary to initialize a new download environment's block for file's download. */

    if ( tmp_block -> filename == NULL ) {

        printf( "\n\n Allocating a new block for file %s in Server's Download Environment (this is the first block).", pathname ); 
        fflush( stdout); 

        ret = init_new_block( tmp_block , pathname, client_address, len );
        if (ret == -1) {
            printf("Error in function: init_new_block (start_download).");
            return -1;
        }

        return 0;
    }


    block               *last_block = NULL;

    do {

        if ( strcmp( pathname, ( tmp_block -> filename ) ) == 0 ) {

            /* if the requested file to download is already present in one of the existing blocks
               of download environment, then the first paused worker of that block is chosen to
               serve this request. */

            worker *tmp_worker = ( tmp_block -> workers );

            do {

                if ( tmp_worker -> is_working == '0' ){

                    //awake the sleeping thread and request the service.

                    tmp_worker -> client_addr = client_address;

                    ret = pthread_kill( ( tmp_worker -> tid ) , SIGUSR1 );
                    if (ret != 0) {
                        printf("Error in function: pthread_kill (start_download).");
                        return -1;
                    }

                    return 0;

                }

            } while ( ( tmp_worker = ( tmp_worker -> next ) ) != NULL);

            printf("\nOps! all block's threads are already in use. Not able to serve a request in an existing block... ");

        }


        last_block = tmp_block;


    } while ( ( tmp_block = (tmp_block -> next ) ) != NULL );


    /*  if the following block of code is going to be executed, it means there are not available blocks 
        (or free workers) to serve the download request on this file. 
        A new block related to the requested file is going to be allocated, so that this request can be
        handled. */

    printf( "\n\n Allocating a new block for file %s in Server's Download Environment...", pathname ); 
    fflush( stdout );

    ret = init_new_block( last_block -> next, pathname, client_address , len );
    if (ret == -1) {
        printf("Error in function: init_new_block (start_download).");
        return -1;
    }

    return 0;

}



        /*   ****    ***     THREADS' FUNCTIONS     ***  ****   */

        

/*  
    This is block's worker-threads' function. 
    This function splits each thread's life into two main parts, one of pause (sleeping), the other concerning transmissions of requested files. 
    As a thread is awaken, it starts forwarding packets till the whole file is transferred.
    Once a thread ends its job, it alerts the parent-block  and increment block's accesses value, then go back to pause.
*/
void * work ( void * _worker ) {

    signal( SIGUSR1, wake_up);              //Set the waking up event handler.
    signal( SIGUSR2, incoming_ack);         //Set the ACKNOWLEDGMENT event handler (signal has to be forwarded by a specialized thread, working for all block's workers).

    int ret;

    worker  * me = ( worker * ) _worker;

    block   * myblock = ( me -> my_block );

    if ( me -> is_working == '0') {
        //printf("\n WORKER %d CREATED BUT TEMPORARILY PAUSED.\n ", me -> identifier ); fflush(stdout);
        goto sleep;
    } else{
        printf("\n WORKER %d RUNNING FOR DOWNLOAD.\n ", me -> identifier ); fflush(stdout);
    }

    redo:

    myblock -> BLTC ++;

    ret = reliable_file_forward( (me -> identifier), ( me -> sockfd ), ( me -> client_addr ), ( me -> len), 
                                    ( myblock -> buffer_cache ), ( me -> sliding_window_slot_ ), &( me -> s_window_mutex ) );
    if (ret == -1) {
        printf("Error in function : reliable_data_trasnfer.");
        goto redo;
    }

    sleep:

    me -> is_working = '0';

    pause();

    goto redo;


}



/*  
    This is the block's acknowledgment keeper (and demultiplexer) thread function.
    By this function, this thread receives block's client's acknowledgments and executes demultiplexing of them: 
    each ACK is directed to a specific block's worker, and further to a specific slot of its sliding window.
    This thread is responsible for notifying workers about the received ACKs and for awakening a worker waiting for sliding his window on.
*/
void * acknowledgment_keeper( void * _block ){


    int     ret,    len = sizeof( struct sockaddr_in );

    char    *id,            *seq_num;


    char    *buffer = malloc( sizeof( char ) * MAXLINE );

    block   *myblock = ( block * ) _block;

    worker  *w_tmp = ( myblock -> workers );

    sw_slot *sw_tmp;

    printf("\n ACKNOWLEDGMENT KEEPER RUNNING FOR BLOCK MATCHED TO FILE : %s\n ", myblock -> filename ); fflush(stdout);

    do {

        /*  Receive a packet (acknowledgments) from related block's socket.  */

        struct sockaddr_in     client_address;

        memset( buffer, 0, sizeof( buffer ) );

        ret = recvfrom( myblock -> server_sock_desc, (char *) buffer, MAXLINE , MSG_WAITALL, ( struct sockaddr *) &client_address, &len); 
        if (ret <= 0)       Error_("Error in function : recvfrom (acknowledgment_demultiplexer).", 1);

        printf("\n ACK received : ");

        /*  Parse the packet to keep separated the identifier and sequence number fields.  */

        id = strtok( buffer, "/" );
        if (ret == -1)      Error_("Error in function sprintf (acknowledgment_demultiplexer).", 1);

        seq_num = strtok( NULL, "/" ) ;
        if (ret == -1)      Error_("Error in function sprintf (acknowledgment_demultiplexer).", 1);


        /*  Find the block's worker with identifier as specified on ACK. */

        while ( ( w_tmp -> identifier ) != atoi(id) ) {

            w_tmp = ( w_tmp -> next );

        }


        /*  Once the worker is found, find the worker's window's slot with sequence number as specified on ACK  */

        sw_tmp = ( w_tmp -> sliding_window_slot_ );

        while ( ( sw_tmp -> sequence_number ) != atoi(seq_num) ) {

            sw_tmp = ( sw_tmp -> next );

        }


        {
            //pthread_mutex_lock( &( w_tmp -> s_window_mutex) );

            /*  THIS IS A CRITICAL SECTION FOR ACCESS ON THE SLIDING WINDOW (shared by ack-keeper thread and the relative worker).
                Update worker window's slot's status from SENT to ACKED. 
                If the slot is the first of the sliding window, forward a SIGUSR2 signal to worker-thread to get the window sliding on. */

            if ( ( sw_tmp -> status ) != SENT )     Error_("Error in acknowledgment handling : unexpected window's status.", 1);

            sw_tmp -> status = ACKED;

            printf(" %d", sw_tmp -> sequence_number );                  fflush(stdout);

            //current_timestamp( sw_tmp -> acked_timestamp );

            if ( ( sw_tmp -> is_first ) == '1' )    {
                pthread_kill( ( w_tmp -> tid ), SIGUSR2 ); 
                printf("\n SIGNAL THE WORKER TO SLIDE ON.");            fflush(stdout);
            }

            //pthread_mutex_unlock( &( w_tmp -> s_window_mutex) );

        }         


    } while (1);

}




/*
    This is the thread function of the time_wizard related to a worker.
    This function implements the retransmission of packets lost within the network, during a worker's file transfer to a client.
    In this function, the thread executes a while(1) loop : for a time equal to the nanoseconds specified in global extern variable "beat", this thread sleeps.
    Every time the thread awakes, it accesses to the worker's sliding window and check the timeout interval on beeing run out :
    If this condition is verified, then the thread executes a retransmission of the specific window slot's packet to the client.
    The sliding window is of course accessed on all of its slots.
*/
void * time_wizard( void * _worker ){

    int                 ret;

    worker              *wrkr = ( worker *) _worker;

    sw_slot             *window = wrkr -> sliding_window_slot_;


    struct timespec     *now = malloc( sizeof( struct timespec ) );
    if (now == NULL)        Error_("Error in function : malloc ( time wizard).", 1);

    if ( wrkr -> is_working == '0') {

        //printf("\n TIME WIZARD %d CREATED BUT TEMPORARILY PAUSED.\n ", wrkr -> identifier ); fflush(stdout);
        pause();                    //wait for a SIGUSR to be awaken (as well as his matched worker thread).

    } else{
        printf("\n TIME WIZARD %d RUNNING.\n ", wrkr -> identifier );                        fflush(stdout);
        pause();           //wait for (the other!) SIGUSR to be awaken (as the worker is ready to transmit).

    }

    do {

        ret = nanosleep( &beat, NULL );
        if (ret == -1)      Error_( "Error in function : nanosleep() (time_wizard).", 1);

        current_timestamp( now );

        for (int i = 0; i < WINDOW_SIZE; i ++ ) {

            if ( ( window -> sent_timestamp ) -> tv_sec != 0  &&  ( window -> sent_timestamp ) -> tv_nsec != 0 ) {
                
                if ( nanodifftime( now, window -> sent_timestamp )  >= ( window -> timeout_interval ) ){

                    if ( retransmission( window, wrkr -> sockfd, (wrkr -> client_addr) ) == -1 )     Error_("Error in function: retransmission (time_wizard).", 1);

                }

            }

            window = ( window -> next );

        }

    } while(1);

}
 





