#include "/mnt/hgfs/shared/HW1/tools/syscall_statistic.h"


int main(){
	int num_procs;
	int i;
	struct switch_info my_ptr[150];

	num_procs = get_scheduling_statistic(my_ptr);
	printf("num procs : %d\n", num_procs);
	
	printf("previous_pid \t next_pid \t previous_policy \t next_policy \t time \t reason \n");
	for(i=0; i<num_procs; i++){
				printf("%d \t %d \t %d \t %d \t %d \t ",
				my_ptr[i].previous_pid, my_ptr[i].next_pid, my_ptr[i].previous_policy, 
				my_ptr[i].next_policy, my_ptr[i].time);
		switch(my_ptr[i].reason){
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
