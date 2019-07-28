/* Server side implementation of RUFT : RECEPTION ENVIRONMENT */

#include "header.h"
#include "Download_Environment.c"
#include "Reliable_Data_Transfer.c"

#define PORT     8080 


#define LIST    0

#define GET     1

#define PUT     2



int                     sockfd; 
struct sockaddr_in      servaddr;



struct client_ {

    pthread_t                   tid;

    struct sockaddr_in          client_address;

    int                         len;

    char                        incoming_request[MAXLINE];

    char                        pathname[MAXLINE];

    char                        buffer[MAXLINE];

    struct client_              *next;

};                                                              struct _client      *clients;





/* This is a buffer cache containing all file names stored in RUFT Server's directory. */
char *                  list;



/* REQUEST HANDLERS DECLARATION */

int     list_request_handler( struct client_ * client_infos );

int     download_request_handler( struct client_ * client_infos );

int     upload_request_handler( struct client_ * client_infos );



/* AUXILIAR FUNCTIONS DECLARATION */

int     current_dir_size();

char    *load_dir( int size );


/* RECEPTIONIST THREAD FUNCTION DECLARATION */

void * receptionist( void * client_info );


  



int main(int argc, char ** argv) { 

    int ret;

    /* Load the current directory list on RAM to be ready for using.  */
    printf("\nRUFT SERVER TURNED ON.\nloading directory file list on buffer cache...");
    int dir_size = current_dir_size();
    list = load_dir( dir_size );
    printf("done. Current directory content is :\n\n%s\n", list);
      

    /* Creating socket file descriptor */ 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
      
    memset(&servaddr, 0, sizeof(servaddr)); 
      
    /* Filling server information  */
    servaddr.sin_family      =    AF_INET;                  // IPv4 
    servaddr.sin_addr.s_addr =    INADDR_ANY;               // 127.0.0.1
    servaddr.sin_port        =    htons(PORT);              // Well-known RUFT Server port.
      
    /* Bind the socket with the server address */ 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    /* Validate the first struct client_ instance of clients dynamic liked list, to be filled by new requests infos. */
    clients = malloc( sizeof( struct client_ ) );              
    
    struct client_      *tmp = clients;

    do{
        /* Within this cycle, RUFT Server receives all types of requests from clients, and creates matched-threads to serve each one of them. */

        //memset( &(tmp -> client_address) , 0, sizeof( tmp -> client_address ) ); 

        printf("waiting for request messages...\n\n");

        /* Receive a msg. Client Address is not specified, it is set at message arrival through transport layer UDP infos. */ 

        memset( tmp -> incoming_request, 0, MAXLINE );

        ret = recvfrom( sockfd, (char *) ( tmp -> incoming_request ) , MAXLINE, MSG_WAITALL, ( struct sockaddr *) &(tmp -> client_address) , &( tmp -> len ) ); 
        if ( ret == -1 ) {
            printf("Error in function : recvfrom() (main).");
            return -1;
        }

        /* Create receptionist thread which starts the serving-job */
        if ( pthread_create( &( tmp->tid), NULL, receptionist, (void *) clients ) == -1 )       Error_("Error in function : pthread_create (main).", 1);
        
        tmp -> next = malloc( sizeof( struct client_ ) );
        if ( tmp -> next == NULL)       Error_("Error in function : malloc (main).", 1);

        tmp = ( tmp -> next );


    } while (1);

     
      
    return 0; 
} 






/* AUXILIAR FUNCTIONS IMPLEMENTATION */

int current_dir_size(){

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

char* load_dir( int size ) {


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

int list_request_handler( struct client_ * client_infos ){

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


int download_request_handler( struct client_ * client_infos ){

    if ( start_download( ( client_infos -> pathname ), ( client_infos -> client_address ) ) == -1 )     return -1;

    return 0;

}






/*
    This is the RECEPTIONIST thread function. A new receptionist thread is created whenever a new request reaches the RUFT server side,
    in order to let the server receiving new requests while this thread is working on this one.
    This function takes as unique parameter a struct client_ in which all client's infos are stored at runtime, at the moment of request's reception.
    This function reads the request and acts different actions depending on request's type.
*/
void * receptionist( void * client_infos ) {

    int ret;

    struct client_      *my_client = (struct client_ *) client_infos;


    /* Elaborates the incoming request packet and switch the operations. */

    char            *tmp;                              int             request_type;

    tmp =  strtok( ( my_client -> incoming_request ), "/");            request_type = atoi(tmp);

    switch( request_type ) {

        case LIST:     /* LIST request */                

            sprintf( ( my_client -> buffer ), "%ld", strlen(list) );

            if ( list_request_handler( my_client ) != 0 )                Error_("Error in function : list_request_handler (receptionist).", 1);
            
            break;

        case GET:     /* GET (DOWNLOAD) request */
            
            tmp = ( my_client -> buffer ) + strlen(tmp);                 memset( ( my_client -> buffer ), 0 , strlen( my_client -> buffer ) );

            if ( sprintf( my_client -> pathname, "%s", tmp) == -1)       Error_("Error in function : sprintf (receptionist).", 1);
            
            if ( download_request_handler( my_client ) != 0 )            Error_("Error in function : download_request_handler (receptionist).", 1);

            break;

        case PUT:

            break;

        default:

            break;

    }

}