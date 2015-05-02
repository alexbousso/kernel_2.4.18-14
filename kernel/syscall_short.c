#include <linux/sched.h>
#include <asm/errno.h>

#define CHECK_TASK(arg) do { \
		if (!arg || arg->policy != SCHED_SHORT) return -EINVAL; \
	} while(0)

/**
 * Returns 1 if the process is a SHORT process or 0 if it is already overdue.
 * If the process is not a SHORT-process, then is returned EINVAL.
 */
int is_SHORT(int pid) {
	task_t *p = find_task_by_pid(pid);
	CHECK_TASK(p);
	return p->is_overdue ? 0 : 1;
}

/**
 * Returns the time left for the process at the current time slice.
 * For overdue process it should return 0.
 * If the process is not a SHORT-process, then is returned EINVAL.
 */
int remaining_time(int pid) {
	task_t *p = find_task_by_pid(pid);
	CHECK_TASK(p);
	return p->is_overdue ? 0 : p->time_slice * 1000 / HZ;
}

/**
 * Returns the number of trials left for the SHORT process. If the process is
 * overdue the function returns 0.
 * If the process is not a SHORT-process, then is returned EINVAL.
 */
int remaining_trials(int pid) {
	task_t *p = find_task_by_pid(pid);
	CHECK_TASK(p);
	return p->is_overdue ? 0 : p->trial_num;
}

int get_scheduling_statistic(struct switch_info *si) {

}

