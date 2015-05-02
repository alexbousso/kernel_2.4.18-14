#include 'syscall_short.h'
#include 'syscall_statistic.h'

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
	int i;
	struct switch_info my_ptr[150] = malloc(sizeof(struct switch_info));
	get_scheduling_statistic(my_ptr);
	for(i=0; i<150; i++){
		printf("previous_pid : %d, previous_policy: %d, next_pid : %d, next_policy : %d, time : %d , reason %s\n" my_ptr[i].previous_pid, my_ptr[i].previous_policy, my_ptr[i].next_pid,  my_ptr[i].next_policy, my_ptr[i].time, reason_txt(my_ptr[i].reason))
	}
	free(my_ptr);

	return 0;
}

struct switch_info{
	int previous_pid;
	int next_pid;
	int previous_policy;
	int next_policy;
	unsigned long time;
	int reason;
};