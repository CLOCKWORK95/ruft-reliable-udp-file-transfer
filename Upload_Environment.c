#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/mman.h>



/* UPLOAD ENVIRONMENT: here are all the functions used for the "PUT" operation

 Client side: multiple clients can send multiple requests. 
																					   WORKERS												Server side
 () 			  ___________________   _______ _____________
 /\    Client 1  |  file "hello.txt" | |   file "Iliad.txt"  | -----------------> Thread_client_1.1	 ------------	CLIENT
/__\             |___________________| |_____________________| -----------------> Thread_client_1.2	 --------	|	SOCKET
																											|	|     __
 ()				  ________________________																	|   |____|  \
 /\    Client 2  |  file "Dear diary.txt" | ------------------------------------> Thread_client_2 ________  |________|   \
/__\             |________________________|																  |__________|    >      ------>>
																													 |   /
 ()				  _______________________																	  _______|  /
 /\    Client 3  |  file "PAPEROGA.txt"  |  ------------------------------------> Thread_client_3 ___________|		 '--
/__\             |_______________________|

*/

#define BUF_SIZE 128 //every char takes 4KB so the filename can be of 32 chars maximum

typedef struct worker_{

    pthread_t               time_wizard;                                //Thread Identifier of this worker's Time_Wizard (who handles timeout-retransmission).

    int                     identifier;                                 //Unique identifier of the worker, to receive ACKs.

    pthread_t               tid;                                        //Identifier of the working-thread.

    struct sockaddr_in      server_addr;                                //Address of the server.

    int                     sockfd;                                     //Socket descriptor to send packets through.
    
   char                    is_working;                                 // '0' : sleeping   |   '1' : working. */

    sw_slot                 *sliding_window_slot_;                      //Circular linked list of sliding window's slots, related to this worker instance.

}               worker;


/* Functions declaration */

int start_upload(int ups_number);
void *thread_func(void *thefile);

char **mem;

int main(){   
 
	int ret,	number_of_files,	*number;
	int i;
	
	pthread_t ack_keeper_tid; //there's only one ack keeper for all the client-side upload environment.
	
	/*  Creates the Acknowledgement Multiplexer Thread, to handle acknowledgements. */
    ret = pthread_create( &ack_keeper_tid , NULL, acknowledgement_demultiplexer, (void *) /*something to pass*/ );
    if (ret ==-1) {
        printf("Error while creating the ack keeper, upload environment client-side.\n");
        return -1;
    }

 
	/* Allocates a dynamic shared buffer to put the incoming filenames into, then every thread takes a filename from the buffer and works with it */
	mem = malloc(sizeof(char *));
 
	while(1){ //multiple requests are supported
	
		printf("Upload Environment client-side is up and running, please enter the number of files to upload on server.\n");
scan:	ret = scanf("%d", number);
		if (ret == 0) {
			printf("Unsupported input.\n");
			goto scan;
		}
		
		number_of_files = strtol(number, NULL, 10);
		if (number_of_files < 1){
			printf("Please enter an integer greater than zero.\n");
			goto scan;
		}
	
		for (i=0; i<number_of_files; i++){
		
			mem[i] = (char *)mmap(NULL, BUF_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_SHARED, 0, 0);
			if (mem[i] == NULL){
				printf("Mapping error.\n");
				return -1;
			}
				
		printf("Please type the name of the file you want to upload: \n");
		ret = scanf("%s", mem[i]);
		if (ret == 0){
			printf("Error: couldn't write the filename into the shared buffer.\n");
			return -1;
		}
		
		start_upload(number_of_files); //number_of_files threads per client-request are activated 

	}

	return 0;
}


int start_upload(int ups_number) {
	
	int i;
	
	//Initialization of the workers starts here
	
	worker *tmp; 
	
	for (i=0; i < ups_number; i++){
		
		//this creates a list of workers (technically a list of structs of type 'worker')
        tmp = malloc( sizeof( worker ) );
        if (tmp == NULL) {
            printf("Error while validating area for the workers.");
            return -1;
        }

        //populate worker's attributes.
        
        tmp -> identifier = i; 

        //tmp -> is_working = '0';

        tmp -> sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        
    }
		
		/*    //////////////////////////////////////////////////////////////////////////////////////

    tmp = ( new_block -> workers ); What happens here?

    tmp -> is_working = '1';

    tmp -> server_addr = server_address;******/////////////////////////////////////////////////////////////////////////////


    /*  Creates and launch all the threads: workers and time wizards */

    for ( i = 0; i < ups_number	; i ++ ) {

        ret = pthread_create( &( tmp -> time_wizard ), NULL, work, (void *) tmp );
        if ( ret == -1 ) {
            printf("Error while creating the time wizards, upload environment client-side.");
            return -1;
        }

        ret = pthread_create( &( tmp -> tid ), NULL, work, (void *) tmp );
        if ( ret == -1 ) {
            printf("Error while creating the workers (threads), upload environment client-side.");
            return -1;
        }

    }
		
		
	return 0;
}



/*  THREADS' FUNCTIONS  */

/* 
    This is the worker function. 
    This function splits each thread's life into two main parts, one of pause (sleeping), the other concerning transmissions of the related file. 
    As a thread is awaken, it starts forwarding packets untill the whole file is transferred. */

void * work ( void * _worker ) {

    signal( SIGUSR_CL1, wake_up);              //Set the waking up event handler.
    signal( SIGUSR_CL2, incoming_ack);         //Set the ACKNOWLEDGEMENT event handler (signal has to be forwarded by a specialized thread, working for all the workers).

    int ret;

    worker  * me = ( worker * ) _worker;

redo:

    ret = reliable_file_transfer( (me -> identifier), ( me -> sockfd ), ( me -> server_addr ),/* ( me -> my_block-> buffer_cache ) */, ( me -> sliding_window_slot_ ) );
    if (ret == -1) {
        printf("Error in function 'reliable_data_transfer', upload environment client-side.");
        goto redo;
    }


    me -> is_working = '0';

    pause();

    goto redo;


}
 
 
 /* 
    This is the thread function of the time_wizard related to a worker.
    This function implements the retransmission of packets lost within the network, during a client worker's file transfer to the server.
    In this function, the thread executes a while(1) loop : for a time equal to the nanoseconds specified in global extern variable "beat", this thread sleeps.
    Every time the thread awakes, it accesses to the worker's sliding window and checks if the timeout interval is elapsed :
    If this condition is verified, then the thread executes a retransmission of the specific window slot's packet to the server.
    The sliding window is of course accessed on all of its slots. */

void * time_wizard( void * _worker ){

    int                 ret;

    worker              *wrkr = ( worker *) _worker;

    sw_slot             *window = wrkr -> sliding_window_slot_;

    struct timespec     *now;

    do {

        ret = nanosleep( &beat, NULL );
        if (ret == -1)      Error_( "Error in function 'nanosleep()' (time_wizard), upload environment client-side.", 1);

        current_timestamp( now );

        for (int i = 0; i < WINDOW_SIZE; i ++ ) {

            if ( ( window -> sent_timestamp ) -> tv_sec != 0  &&  ( window -> sent_timestamp ) -> tv_nsec != 0 ) {
                
                if ( nanodifftime( now, window -> sent_timestamp )  >= ( window -> timeout_interval ) ){

                    if ( retransmission( window, wrkr -> sockfd, &(wrkr -> server_addr) ) == -1 )     Error("Error in function 'retransmission' (time_wizard), upload environment client-side.", 1);

                }

            }

            window = ( window -> next );

        }

    } while(1);

}




    return 0;

} 

