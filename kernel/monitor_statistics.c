#include <linux/monitor_statistics.h>

struct switch_info MonitorArray[MAX_PROC_ARR_SZ] = {0};	//see why {{}}

int reason_for_switch;
int switches_since_last_task_created_or_died;

int first_cell = 0;
int last_cell = -1;
int finished_one_round = 0;

int next_cell(int k){
	return ((k+1)%MAX_PROC_ARR_SZ);
}

void insert_context_switch(int previous_pid, int next_pid, int previous_policy, int next_policy, unsigned long time, int reason){
	int curr_cell = next_cell(last_cell); 
	
	MonitorArray[curr_cell].previous_pid = previous_pid;
	MonitorArray[curr_cell].next_pid = next_pid;
	MonitorArray[curr_cell].previous_policy = previous_policy;
	MonitorArray[curr_cell].next_policy = next_policy;
	MonitorArray[curr_cell].time = time;
	MonitorArray[curr_cell].reason = reason;

//this keeps the struct of the semi-list array;
	if(last_cell == MAX_PROC_ARR_SZ-1){
		finished_one_round = 1;
	}
	if(finished_one_round){
		first_cell = next_cell(first_cell);
	}	
	last_cell = curr_cell;
}