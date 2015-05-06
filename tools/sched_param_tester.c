/*
 * sched_param_tester.c
 *
 *  Created on: 5 במאי 2015
 *      Author: eytan
 */

#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include "hw2_syscalls.h"

#define SCHED_OTHER 0
#define SCHED_SHORT	4

extern int errno;

int make_busy_short_process() {
	struct sched_param2 params;
	params.requested_time = 5;
	params.sched_priority = 0;
	params.trial_num = 5;
	clock_t ticks;
	int id = fork();
	if (id == 0) {
		id = getpid();
		sched_setscheduler(id, SCHED_SHORT, &params);
		printf("short running");
		while ((double)ticks/CLOCKS_PER_SEC < 500) {
			id = 5 + id;
			ticks = clock();
		}
		printf("short done");
		exit(0);
	} else {
		usleep(300); //less then milisecond to let short finish being short
		return id;
	}
}

int make_empty_process() {
	clock_t ticks;
	int id = fork();
	if (id == 0) {
		sleep(1);
		exit(0);
	} else {
		return id;
	}
}

void print_params(struct sched_param2 params) {
	printf("prio: %d\n", params.sched_priority);
	fflush(stdout);
	printf("req_time: %d\n", params.requested_time);
	fflush(stdout);
	printf("req_trials: %d\n", params.trial_num);
	fflush(stdout);
}

void print_error(const char* string, int use_err) {
	printf("ERROR: %s", string);
	if (use_err) {
		printf(" errno: %d", errno);
	}
	printf("\n");
	fflush(stdout);
}

void check_syscalls() {
	int temp;
	int pid;
	struct sched_param2 params;
	pid = make_empty_process();
	//check fails
	temp = is_SHORT(pid);
	if (temp != -1 || errno != EINVAL) {
		print_error("is_SHORT didnt fail correctly", 1);
	}
	temp = remaining_time(pid);
	if (temp != -1 || errno != EINVAL) {
		print_error("remaining_time didnt fail correctly", 1);
	}
	temp = remaining_trials(pid);
	if (temp != -1 || errno != EINVAL) {
		print_error("remaining_trials didnt fail correctly", 1);
	}
	// check params after a little sleep
	params.sched_priority = 0;
	params.trial_num = 5;
	params.requested_time = 3;
	sched_setscheduler(pid, SCHED_SHORT, &params);
	usleep(4050); // first trial 3, second 1, third 1.
	if (is_SHORT(pid) != 1) {
		print_error("not short for when supposed to be", 0);
	}
	printf("remaining time should be between milisecond to zero probably in the 950 micro seconds.\n");
	printf("remaining time (in miliseconds): %d\n", remaining_time(pid));
	// check overude - sleep enough for overdue but dont oversleep a second
	pid = make_busy_short_process();
	if (is_SHORT(pid) != 0) {
		print_error("problem with overdue is_short", 0);
	}
	if (remaining_time(pid) != 0) {
		print_error("problem with overdue time", 0);
	}
	if (remaining_trials(pid) != 0) {
		print_error("problem with overdue trials", 0);
	}
}

int main(int argc, char * argv[]) {
	int temp;
	int other_max_prio;
	int other_min_prio;
	int short_max_prio;
	int short_min_prio;
    int pid_other;
    int orig_short_time = 10;
    int orig_short_trials = 50;
    int pid_other_to_short;
    int pid_short_to_short_legal_more;
    int pid_short_to_short_legal_less;
    int pid_short_to_short_illegal;
    int pid_short_to_other_illegal;
    struct sched_param2 param_other;
    struct sched_param2 param_short; // ill mix calls to see it doesnt fall

    other_min_prio = sched_get_priority_min(SCHED_OTHER);
    printf("other min prio: %d\n", other_min_prio);
    fflush(stdout);
    other_max_prio = sched_get_priority_max(SCHED_OTHER);
    printf("other max prio: %d\n", other_max_prio);
    fflush(stdout);
    short_min_prio = sched_get_priority_min(SCHED_SHORT);
    printf("short min prio: %d\n", short_min_prio);
    fflush(stdout);
    short_max_prio = sched_get_priority_max(SCHED_SHORT);
    printf("short max prio: %d\n", short_max_prio);
    fflush(stdout);
    if (0 != other_min_prio || 0 != other_max_prio || 0 != short_min_prio || 0 != short_max_prio) {
    	print_error("wrong prio from minmax prio syscall", 0);
    }
    pid_other = make_empty_process();
    sched_getparam(pid_other, &param_other);
    assert(param_other.sched_priority >= other_min_prio ||
    		param_other.sched_priority <= other_max_prio);
    print_params(param_other);
    param_short.sched_priority = 0;
    param_short.requested_time = orig_short_time;
    param_short.trial_num = orig_short_trials;
    pid_other_to_short = make_empty_process();
    if( 0 != sched_setscheduler(pid_other_to_short, SCHED_SHORT, &param_short)) {
    	print_error("set scheduler to short faild when expecting success", 1);
    }
    // now that its short let time pass so maybe a timeslice will end while sleeping - not likely
    param_other.sched_priority = other_max_prio + 1;
    temp =  sched_setscheduler(pid_other, SCHED_OTHER, &param_other);
    if ( 0 <= temp) {
    	print_error("set scheduler to other didnt fail when expecting fail", 0);
    } else if ( errno != EINVAL) {
    	printf("%d\n", temp);
    	fflush(stdout);
    	print_error("wrong errno was set for other, expected EINVAL (use google lazy basterd))", 1);
    }
    // get a short for legal testing. increase decrease requested time
    pid_short_to_short_legal_more = make_empty_process(); //still other
    if( 0 != sched_setscheduler(pid_short_to_short_legal_more, SCHED_SHORT, &param_short)) {
    	print_error("set scheduler to short faild when expecting success", 1);
    }
    param_short.requested_time += 100;
    temp = param_short.requested_time;
    if( 0 != sched_setparam(pid_short_to_short_legal_more, &param_short)) {
    	print_error("set param for short- more time faild when expecting success", 1);
    }
    if (0 != sched_getparam(pid_short_to_short_legal_more, &param_short)) {
    	print_error("get param for short faild when expecting success", 1);
    }
    if (param_short.requested_time != temp || param_short.trial_num != orig_short_trials) {
    	print_error("get param returned incorrect params", 0);
    }

    //get first short, check params, decrease time and check again
    if( 0 != sched_getparam(pid_other_to_short, &param_short)) {
    	print_error("get param for short- more time faild when expecting success", 1);
    }
    if (param_short.requested_time != orig_short_time || param_short.trial_num != orig_short_trials) {
    	print_error("get param returned incorrect params", 0);
    }
    param_short.requested_time = 1;
    temp = param_short.requested_time;
    pid_short_to_short_illegal = pid_other_to_short;
    if( 0 == sched_setscheduler(pid_short_to_short_illegal, SCHED_SHORT, &param_short)) {
    	print_error("set scheduler short to short succeded", 0);
    } else if (errno != EPERM) {
    	print_error("set scheduler short to short wrong errno", 1);
    }
    param_short.requested_time = 0;
    if( 0 == sched_setparam(pid_short_to_short_illegal, &param_short)) {
    	print_error("set param short to short wrong param succeded", 0);
    } else if (errno != EINVAL) {
    	print_error("set param short to short wrong errno", 1);
    }
    sched_getparam(pid_short_to_short_illegal, &param_short);
    if (param_short.requested_time != orig_short_time || param_short.trial_num != orig_short_trials) {
    	print_error("get param returned incorrect params after set sched failed", 0);
    }
    if( 0 != sched_setparam(pid_short_to_short_illegal, &param_short)) {
    	print_error("set param for short- less time faild when expecting success", 1);
    }
    pid_short_to_other_illegal = pid_other_to_short;
    if( 0 == sched_setscheduler(pid_short_to_other_illegal, SCHED_OTHER, &param_other)) {
    	print_error("set scheduler short to other succeded", 0);
    }
    check_syscalls();
    return 0;
}
