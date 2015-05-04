#include <linux/sched.h>
#include <asm/errno.h>
#include <linux/monitor_statistics.h>
#include <asm/uaccess.h>


extern struct switch_info MonitorArray[MAX_PROC_ARR_SZ];
extern int last_cell;
extern int first_cell;
extern int finished_one_round;


int get_scheduling_statistic(struct switch_info* input){
	int cells_round1, cells_round2;
	int size_round1, size_round2;
	/*the size of switch_info is 150 */
	if(!input){
		return -1;
	}

	if(!finished_one_round){
		cells_round1 = last_cell - first_cell + 1;
		cells_round2 = 0;
	}else{
		cells_round1 = MAX_PROC_ARR_SZ - first_cell;
		cells_round2 = last_cell + 1;
	}
	size_round1 = (sizeof(struct switch_info))*cells_round1;
	size_round2 = (sizeof(struct switch_info))*cells_round2;

	copy_to_user (input, MonitorArray + first_cell, size_round1);
	copy_to_user (input + cells_round1, MonitorArray, size_round2);
	//copy_to_user(to, from, size in unsigned long)	
	
	if(finished_one_round){
		return MAX_PROC_ARR_SZ;
	}
	
	return (last_cell - first_cell + 1);
	
	//copy_to_user (input, MonitorArray, MAX_PROC_ARR_SZ*sizeof(struct switch_info));
	//return last - first +1;
	
}
