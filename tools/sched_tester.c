/*
 * sched_tester.c
 *
 *  Created on: 2 במאי 2015
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
#include "syscall_statistic.h"

/*struct switch_info{
	int previous_pid;
	int next_pid;
	int previous_policy;
	int next_policy;
	int time;
	int reason;
};*/

/*
struct sched_param{
	int sched_priority;
	int requested_time;
	int trial_num;
};
*/
struct sched_param2 {
	int sched_priority;
	int requested_time;
	int trial_num;
};

int fibonacci(int n){
	if (n < 2){
		return n;
	}
	return fibonacci(n-1) + fibonacci(n-2);
}


int main(int argc, char *argv[]) {


	if (argc % 2 != 1){
		printf("bad arguments num! \n");
		return 0;
	} //should be odd number of arguments

	printf("\nAlex and Tzoof\n");
	printf("\nPid\tNum of trials\t\tn\tThis pid: %d\n", getpid());
	for(int i=0; i<100; i++){
		;
	}
	for (int i = 1; i < argc; i+=2){
		char *end1, *end2;
		long int numOfTrials = strtol(*(argv+i), &end1, 10);
		long int n = strtol(*(argv+i+1), &end2, 10);

		if (*end1 || *end2 || numOfTrials < 0 || n < 0){
			printf("bad arguments num! \n");
			return -1;
		}

		if (fork() == 0){
			printf("\nI'm forking\n");
			printf("%d\t%lu\t%lu\n", getpid(), numOfTrials, n);
			for(int i=0; i<100; i++){
				;
			}
			struct sched_param2 param;
			param.sched_priority = 0;
			param.requested_time = 2000; // <-----------------------------------
			param.trial_num = numOfTrials;
			sched_setscheduler(getpid(), 4,(struct sched_param*) &param);
			fibonacci(n);
			return 0;
		}
	}

	for (int i = 1; i < argc; i+=2){
		wait(0);
	}

	struct switch_info res[150];
	int num_procs = get_scheduling_statistic(res);

	printf("i\tprev_pid\tnext_pid\tprev_policy\tnext_policy\ttime\treason\n");
	for(int i=0; i<num_procs; i++){
				printf("%d\t%d\t%d\t%d\t%d\t%lu\t", i,
				res[i].previous_pid, res[i].next_pid, res[i].previous_policy,
				res[i].next_policy, res[i].time);
		switch(res[i].reason){
		case 1:
			printf("task created\n");
			break;
		case 2:
			printf("task ended\n");
			break;
		case 3:
			printf("task yields\n");
			break;
		case 4:
			printf("short became overdue\n");
			break;
		case 5:
			printf("went to wait\n");
			break;
		case 6:
			printf("return from wait\n");
			break;
		case 7:
			printf("time slice ended\n");
			break;
		default:
			printf("**error**\n");
			break;
		}
	}

	return 0;
}
