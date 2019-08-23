/* Server side implementation of RUFT : RECEPTION ENVIRONMENT */

#include "header.h"
#include "Download_Environment.c"
#include "Upload_Environment.c"
#include "Reliable_Data_Transfer.c"


#define PORT     5193


#define LIST    0

#define GET     1

#define PUT     2



int                     sockfd;                     struct sockaddr_in      servaddr;



struct client_ {

    pthread_t                   receptionist;

    struct sockaddr_in          client_address;

    int                         len;

    char                        incoming_request[MAXLINE];

    char                        *pathname;

    char                        buffer[MAXLINE];

    struct client_              *next;

};                                                                  struct client_      *clients;





/* This is a buffer cache containing all file names stored in RUFT Server's directory. */

char                            *list;



/* REQUEST HANDLERS DECLARATION */

int     list_request_handler( struct client_ * client_infos );

int     download_request_handler( struct client_ * client_infos );

int     upload_request_handler( struct client_ * client_infos );


/* AUXILIAR FUNCTIONS DECLARATION */

int     current_dir_size();

char    *load_dir( int size );

int     initialize_upload_environment();


/* RECEPTIONIST THREAD FUNCTION DECLARATION */

void    * receptionist( void * client_info );





int     main( int argc, char ** argv ) { 

    int ret;

    /* Load the current directory list on RAM to be ready for using.  */
    printf( "\n RUFT SERVER TURNED ON.\n Loading directory file list on buffer cache..." );
    int dir_size = current_dir_size();
    list = load_dir( dir_size );
    printf("Done.\n Current directory content is :\n\n%s\n", list );        fflush(stdout);
      

    /* Creating socket file descriptor */ 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
      
    memset(&servaddr, 0, sizeof(servaddr)); 
      
    /* Filling server information  */
    servaddr.sin_family      =    AF_INET;                          // IPv4 
    servaddr.sin_addr.s_addr =    INADDR_ANY;                       // 127.0.0.1
    servaddr.sin_port        =    htons(PORT);                      // Well-known RUFT Server port.
      
    /* Bind the socket with the server address */ 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr) ) < 0 ) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    /* Validate the first struct client_ instance of clients dynamic liked list, to be filled by new requests infos. */
    clients = malloc( sizeof( struct client_ ) );  
    if ( clients == NULL )      Error_("Error in function : malloc (main.)", 1);

    /* Initialization of Upload Environment. */

    ret = initialize_upload_environment();
    if (ret == -1) {
        printf("\n Error in function : initialize_upload_environment (main).");
        return -1;
    }

    /* Initialization of Download Environment, by validating memory area for its first block and for the block's first worker. */            
    download_environment = malloc( sizeof( block ) );
    if ( download_environment            == NULL )       Error_("Error in function : malloc (main.)", 1);

    download_environment -> workers = malloc( sizeof( worker ) );        
    if ( download_environment -> workers == NULL )       Error_("Error in function : malloc (main.)", 1);

    download_environment -> filename = NULL;
    
    struct client_      *tmp = clients;

    do{
        /*  Within this cycle, RUFT Server receives all types of requests from clients, 
            and creates matched-threads to serve each one of them. */

        tmp -> len = sizeof( tmp -> client_address );

        printf("\n Waiting for request messages...\n\n");                                 fflush(stdout);

        /*  Receive a msg. Client Address is not specified, it is set at message arrival through transport layer UDP infos. */ 

        ret = recvfrom( sockfd, (char *) ( tmp -> incoming_request ), MAXLINE, MSG_WAITALL, 
                            ( struct sockaddr *) &( tmp -> client_address ), &( tmp -> len ) ); 
        if ( ret == -1 ) {
            printf("Error in function : recvfrom() (main). errno = %d", errno );
            return -1;
        }

        /*  Create receptionist thread which starts the serving-job */
        if ( pthread_create( &( tmp -> receptionist), NULL, receptionist, (void *) tmp ) == -1 )       
                                                 Error_("Error in function : pthread_create (main).", 1);
        
        tmp -> next = malloc( sizeof( struct client_ ) );
        if ( tmp -> next == NULL)                Error_("Error in function : malloc (main).", 1);

        tmp = ( tmp -> next );

    } while (1);

    return 0; 

} 






/* AUXILIAR FUNCTIONS IMPLEMENTATION */

int     current_dir_size() {

    int     size = 1;

    DIR     *d;

    struct dirent *dir;

    d = opendir("./server_directory");

    if (d) {

        while ( ( dir = readdir(d) ) != NULL ) {

            size += ( strlen( dir -> d_name ) + 1 );

        }
    }

    closedir(d);

    return size;
}

char    * load_dir( int size ) {


    DIR     *d;

    struct dirent *dir;

    d = opendir("./server_directory");

    if ( d != NULL ) {

        list = malloc( sizeof(char) * ( size ) );

        char * tmp = list;

        while ( ( dir = readdir( d ) ) != NULL ) {

            sprintf( tmp, "%s\n", ( dir -> d_name ) );

            tmp += strlen(tmp);

        }

    }

    closedir(d);

    return list;
  
}






/* REQUEST HANDLERS IMPLEMENTATION */

int     list_request_handler( struct client_ * client_infos ) {

    int ret;   

    /* Send the directory's size to the client. */
    ret = sendto( sockfd, (const char *) ( client_infos -> buffer ), strlen( client_infos -> buffer ), 
                  MSG_CONFIRM, (const struct sockaddr *) &( client_infos -> client_address ), ( client_infos -> len ) );
    if (ret == -1)      Error_("Error in function : sendto (main).", 1);

    /* Send the directory's file names list to the client. */
    ret = sendto( sockfd, (const char *) list, strlen(list), MSG_CONFIRM, (const struct sockaddr *) &( client_infos -> client_address ), ( client_infos -> len ) );
    if (ret == -1)      Error_("Error in function : sendto (main).", 1);

    return 0;

}





/*
    This is the RECEPTIONIST thread function. A new receptionist thread is created whenever a 
    new request reaches the RUFT server side, in order to let the server receiving new requests while 
    this thread is working on this one. This function takes as unique parameter a struct client_ 
    in which all client's infos are stored at runtime, at the moment of request's reception.
    This function reads the request and acts different actions depending on request's type.
*/
void    * receptionist( void * client_infos ) {

    int ret;

    struct client_      *my_client = (struct client_ *) client_infos;


    /* Elaborates the incoming request packet and switch the operations. */

    char            *tmp;                              int              request_type;

    int             dir_size;

    tmp =  strtok( ( my_client -> incoming_request ), "/" );            request_type = atoi( tmp );

    switch( request_type ) {

        case LIST:     /* LIST request          */     
 
            /*  Update the list before forwanding it. */
            dir_size = current_dir_size();                             
            list = load_dir( dir_size );           

            sprintf( ( my_client -> buffer ), "%ld", strlen(list) );

            if ( list_request_handler( my_client ) != 0 )                
                                                        Error_("Error in function : list_request_handler (receptionist).", 1);

            break;

        case GET:     /* GET (DOWNLOAD) request */
            
            tmp = ( my_client -> incoming_request ) + (strlen(tmp) + 1 );       

            printf(" pathname received is : %s\n\n", tmp ); fflush(stdout);

            my_client -> pathname = strtok( tmp, "-" );

            if ( start_download( ( my_client -> pathname ), &( my_client -> client_address ), my_client -> len ) == -1 )     
                                                              Error_("Error in function : start download (receptionist).", 1);

            break;

        case PUT:     /* PUT (UPLOAD) request    */

            tmp = ( my_client -> incoming_request ) + (strlen(tmp) + 1 );       

            printf(" pathname received is : %s\n\n", tmp ); fflush(stdout);

            my_client -> pathname = strtok( tmp, "-" );

            if ( start_upload( ( my_client -> pathname ), &( my_client -> client_address ), my_client -> len ) == -1 )     
                                                                Error_("Error in function : start upload (receptionist).", 1);

            break;

        default:

            break;

    }

    pthread_exit( NULL );

}