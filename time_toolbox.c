#pragma once
#include "header.h"


/*  Flag to establish if a RUFT server instance is using default timeout interval or adaptive one.  */
char                    ADAPTIVE; 

struct timespec         beat = { 0, 3000000 };

long                    TIMEOUT_INTERVAL =  (long) 5000000;

long                    EstimatedRTT = 0;

long                    DevRTT = 0;






int current_timestamp( struct timespec* time ) {

	if( clock_gettime( CLOCK_MONOTONIC , time ) == -1 ) {
        printf("Error in function : clock_gettime() : current_timestamp");
        return -1;
    }

    return 0;

}


void print_elapsed( struct timespec *start, struct timespec *stop ) {

    struct timespec *result = malloc( sizeof( struct timespec ) );

    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }

    printf("\n \n TRANSACTION TIME : %.5f seconds", ( (double) result -> tv_sec + 1.0e-9 * result -> tv_nsec ) );

    free( result );

    return;

}


long nanodifftime ( struct timespec *time1, struct timespec *time2 ){

    long time_1 = ( time1 -> tv_nsec ) + ( (1.0e9) * ( time1 -> tv_sec ) );

    long time_2 = ( time2 -> tv_nsec ) + ( (1.0e9) * ( time2 -> tv_sec ) );

    long nanodiff = time_1 - time_2;

    return nanodiff;

}



