#include <errno.h>

struct switch_info{
	int previous_pid;
	int next_pid;
	int previous_policy;
	int next_policy;
	unsigned long time;
	int reason;
};

int get_scheduling_statistic(struct switch_info* input){
	long __res;
	__asm__ volatile (
	"movl $246, %%eax;"
	"movl %1, %%ebx;"
	"int $0x80;"
	"movl %%eax,%0"
	: "=m" (__res) 		/* output operands */
	: "m" (input)		/* input operands */
	: "%eax","%ebx"		/* list of clobbered registers */
	);
	if ((unsigned long)(__res) >= (unsigned long)(-125)) {
	errno = -(__res); __res = -1;
	}
	return (int)(__res);
}
