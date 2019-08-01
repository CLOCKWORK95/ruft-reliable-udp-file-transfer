#pragma once
#include "header.h"

struct timespec         beat = { 0, 70000000 };
long                    TIMEOUT_INTERVAL = (long) 1000000000;


int current_timestamp( struct timespec* time ) {
    
	time->tv_sec = 0;
	time->tv_nsec = 0;


	if( clock_gettime( CLOCK_MONOTONIC_RAW ,time) == -1){
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