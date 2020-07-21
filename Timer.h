#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>


void setTimer(struct timeval* timeOut){
    gettimeofday(timeOut, NULL);
    timeOut->tv_usec = timeOut->tv_usec + 500000; //add 0.5 sec or 500000 microseconds
}
void setTimer2sec(struct timeval* timeOut){
    gettimeofday(timeOut, NULL);
    timeOut->tv_sec = timeOut->tv_sec + 2; //add 2 sec
}

int isTimeOut(struct timeval* cur_time, struct timeval* timeOut){
    gettimeofday(cur_time, NULL);
    double cur = (double)cur_time->tv_sec + (double)cur_time->tv_usec/1000000;
    double limit = (double)timeOut->tv_sec + (double)timeOut->tv_usec/1000000;
    //printf("cur: %f, limit: %f, time left until timeout: %f\n", cur, limit, limit - cur );
    if(cur < limit){
        return 0;
    }
    else{
        return 1;
    }
}
