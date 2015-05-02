/*
 * sched_tester.c
 *
 *  Created on: 2 áîàé 2015
 *      Author: Shani
 */

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "syscall_short.h"

int fibonacci(int n){
	if (n < 2){
		return n;
	}
	return fibonacci(n-1) + fibonacci(n-2);
}
struct sched_param2 {
	int sched_priority;
	int requested_time;
	int trial_num;
};

int main(int argc, char *argv[]) {
	if (argc % 2 != 1){
		printf("bad arguments num! \n");
		return 0;
	} //should be odd number of arguments

	for (int i = 1; i < argc; i+=2){
		char *end1, *end2;
		long int numOfTrials = strtol(*(argv+i), &end1, 10);
		long int n = strtol(*(argv+i+1), &end2, 10);

		if (*end1 || *end2 || numOfTrials < 0 || n < 0){
			printf("bad arguments num! \n");
			return -1;
		}
		//printf("trials = %d, n = %d, errno = %d\n", numOfTrials, n, errno);

		if (fork() == 0){
			struct sched_param2 param;
			param.sched_priority = 0;
			param.requested_time = 10; // <-----------------------------------
			param.trial_num = numOfTrials;
			sched_setscheduler(getpid(), 4,(struct sched_param*) &param);
			fibonacci(n);
			printf("%d ended!\n", getpid());
			break;
		}
	}

	for (int i = 1; i < argc; i+=2){
		wait(0);
	}
	return 0;
}
