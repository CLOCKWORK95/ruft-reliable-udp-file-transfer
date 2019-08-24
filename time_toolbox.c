#pragma once
#include "header.h"


/*  Flag to establish if a RUFT server instance is using default timeout interval or adaptive one.  */
char                    ADAPTIVE; 

struct timespec         beat = { 0, 70000000 };

long                    TIMEOUT_INTERVAL =  (long) 1000000000;

long                    EstimatedRTT = 0;

long                    DevRTT = 0;






int current_timestamp( struct timespec* time ) {

	if( clock_gettime( CLOCK_MONOTONIC_RAW , time) == -1) {
        printf("Error in function : clock_gettime() : current_timestamp");
        return -1;
    }

    return 0;

}



long nanodifftime ( struct timespec *time1, struct timespec *time2 ){

    long time_1 = ( time1 -> tv_nsec ) + ( (1.0e9) * ( time1 -> tv_sec ) );

    long time_2 = ( time2 -> tv_nsec ) + ( (1.0e9) * ( time2 -> tv_sec ) );

    long nanodiff = time_1 - time_2;

    return nanodiff;

}



