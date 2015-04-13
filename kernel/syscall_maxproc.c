#include <linux/sched.h>
#include <asm/errno.h>


int set_child_max_proc(int maxp) {
	if (maxp < 0) {
		current->is_defined_child_max = 0;
		return 0;
	}
	if (current->is_defined_my_max && maxp >= current->my_max) {
		current->is_defined_child_max = 0;
		return -EPERM; // What to return?
	}
	current->is_defined_child_max = 1;
	current->child_max = maxp;
	return 0;
}

int get_max_proc(){
	return (current->is_defined_my_max)? current->my_max: -EINVAL ;
}

int get_subproc_count(){
	return current->tasks_count;
}