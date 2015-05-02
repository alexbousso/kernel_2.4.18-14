#include "/mnt/hgfs/shared/HW1/tools/syscall_statistic.h"

char* reason_txt(int num){
	switch(num){
		case 1:
			return "task created";
		case 2: 
			return "task ended";
		case 3:
			return "task yields";
		case 4:
			return "short became overdue";
		case 5:	
			return "went to wait";
		case 6:
			return "return from wait";
		case 7:
			return "time slice ended";
	}
}
int main(){
	int num_procs;
	int i;
	struct switch_info my_ptr[150];
	num_procs = get_scheduling_statistic(my_ptr);
	printf("num procs : %d\n", num_procs);
	for(i=0; i<num_procs; i++){
		printf("previous_pid : %d, previous_policy: %d, next_pid : %d, next_policy : %d, time : %d , reason : %s\n", 
				my_ptr[i].previous_pid, my_ptr[i].previous_policy, my_ptr[i].next_pid, 
				my_ptr[i].next_policy, my_ptr[i].time, reason_txt(my_ptr[i].reason));
	}
	//free(my_ptr);

	return 0;
}
