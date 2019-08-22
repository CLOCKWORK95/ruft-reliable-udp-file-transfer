#pragma once
#define _POSIX_C_SOURCE 199309L
#include <signal.h>
#include <stdio.h> 
#include <pthread.h>
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <dirent.h>
#include <time.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/sem.h>
#include <sys/ipc.h>


#define Error_(x,y)     { puts(x); exit(y); }
#define MAXLINE 1024 

extern long                TIMEOUT_INTERVAL;
extern struct timespec     beat;


