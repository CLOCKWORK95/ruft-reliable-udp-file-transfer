/* Server side implementation of RUFT : RECEPTION ENVIRONMENT */

#include "header.h"
#include "Download_Environment.c"
#include "Reliable_Data_Transfer.c"

#define PORT     8080 

int                     sockfd; 
char                    buffer[MAXLINE],       msg[MAXLINE];
struct sockaddr_in      servaddr,              client_address;


/* This is a buffer cache containing all file names stored in RUFT Server's directory. */
char *                  list;



/* REQUEST HANDLERS DECLARATION */

int     list_request_handler( char * buffer, struct sockaddr_in *clientaddr );

int     download_request_handler( char * pathname, struct sockaddr_in clientaddr );

int     upload_request_handler( char * buffer );



/* AUXILIAR FUNCTIONS DECLARATION */

int     current_dir_size();

char    *load_dir( int size );


  



int main(int argc, char ** argv) { 

    int ret;

    /* Load the current directory list on RAM to be ready for using.  */

    printf("\nRUFT SERVER TURNED ON.\nloading directory file list on buffer cache...");
    int dir_size = current_dir_size();
    list = load_dir( dir_size );
    printf("done. Current directory content is :\n\n%s\n", list);
      

    // Creating socket file descriptor 

    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
      
    memset(&servaddr, 0, sizeof(servaddr)); 
      
    // Filling server information 
    servaddr.sin_family      =    AF_INET;                  // IPv4 
    servaddr.sin_addr.s_addr =    INADDR_ANY;               // 127.0.0.1
    servaddr.sin_port        =    htons(PORT);              // Well-known RUFT Server port.
      
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
      
    int len; 

    do{

        memset(&client_address, 0, sizeof(client_address)); 

        printf("waiting for request messages...\n\n");

        /* Receive a msg. Client Address is not specified, it is set at message arrival through transport layer UDP infos. */ 

        ret = recvfrom( sockfd, (char *) buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &client_address, &len); 
        if ( ret == -1 ) {
            printf("Error in function : recvfrom() (main).");
            return -1;
        }


        /* Elaborate the request packet and switch the operations. */

        char            *tmp;                   int             request_type;

        tmp =  strtok( buffer, "/");            request_type = atoi(tmp);

        switch( request_type ) {

            case 0:                      

                memset(buffer, 0 , strlen(buffer));         sprintf( buffer, "%ld", strlen(list));

                /* Send the directory's size to the client. */
                ret = sendto( sockfd, (const char *) buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *) &client_address, len );
                if (ret == -1)      Error_("Error in function : sendto (main).", 1);

                /* Send the directory's file names list to the client. */
                ret = sendto( sockfd, (const char *) list, strlen(list), MSG_CONFIRM, (const struct sockaddr *) &client_address, len );
                if (ret == -1)      Error_("Error in function : sendto (main).", 1);
                
                break;

            case 1:
                
                tmp = buffer + strlen(tmp);                 memset(buffer, 0 , strlen(buffer));
                
                if ( download_request_handler( tmp, client_address ) != 0 )       Error_("Error in function : download_request_handler.", 1);

                break;

            case 2:

                break;

            default:

                break;

        }
          
        
        memset( buffer, 0, strlen(buffer) );


    } while (1);

     
      
    return 0; 
} 





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

int list_request_handler( char * buffer , struct sockaddr_in *clientaddr ){

    int ret;    socklen_t len;

    len = (socklen_t) sizeof( *clientaddr );

    /* Send the directory's size to the client. */
    ret = sendto( sockfd, (const char *) buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *) clientaddr, len );
    if (ret == -1)      Error_("Error in function : sendto (main).", 1);

    /* Send the directory's file names list to the client. */
    ret = sendto( sockfd, (const char *) list, strlen(list), MSG_CONFIRM, (const struct sockaddr *) clientaddr, len );
    if (ret == -1)      Error_("Error in function : sendto (main).", 1);

    return 0;

}


int download_request_handler( char * pathname, struct sockaddr_in clientaddr ){

    if ( start_download( pathname, clientaddr ) == -1 )     return -1;

    return 0;

}