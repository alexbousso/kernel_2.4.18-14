#ifndef _MONITOR_STATISTIC_H
#define _MONITOR_STATISTIC_H

#define MAX_PROC_ARR_SZ 150
#define MAX_PROC_TO_ACTION 30

/*GLOBAL VARIABLES*/
struct switch_info{
	int previous_pid;
	int next_pid;
	int previous_policy;
	int next_policy;
	unsigned long time;
	int reason;
};

/*FUNCTION DECLERATIONS */

int next_cell(int k);

void insert_context_switch(	int previous_pid, int next_pid, int previous_policy, int next_policy, unsigned long time, int reason);

#endif