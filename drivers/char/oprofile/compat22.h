/* COPYRIGHT (C) 2002 Philippe Elie, based on discussion with John Levon
 * stuff here come from various source, linux kernel header, John's trick etc.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* This file is intended to be up-to-date with the last linux version and
 * provide work-around for missing features in previous kernel version */

#ifndef COMPAT22_H
#define COMPAT22_H

#include "apic_up_compat.h"
 
#include <linux/smp_lock.h>

#define pte_page_address(a) pte_page(a)
 
#define GET_VM_OFFSET(v) ((v)->vm_offset) 
#define MODULE_LICENSE(l)
#define NEED_2_2_DENTRIES
#define INC_USE_COUNT_MAYBE MOD_INC_USE_COUNT
#define DEC_USE_COUNT_MAYBE MOD_DEC_USE_COUNT
#define lock_execve lock_kernel
#define unlock_execve unlock_kernel
 
/* BKL-protected on 2.2 */
#define lock_mmap(mm) do {} while (0)
#define unlock_mmap(mm) do {} while (0)
 
/* the wake_up path doesn't disable interrupts for wait queue
 * manipulation. So let's force it to.
 */
static inline void oprof_wake_up(struct wait_queue **q)
{
	unsigned long flags;
	save_flags(flags);
	cli();
	wake_up(q);
	restore_flags(flags);
}

extern int wind_dentries_2_2(struct dentry *dentry);
extern uint do_path_hash_2_2(struct dentry *dentry);
#define wind_dentries(d, v, r, m) wind_dentries_2_2(d)
#define hash_path(f) do_path_hash_2_2((f)->f_dentry)

static inline void lock_out_mmap(void)
{
	lock_kernel();
	down(&current->mm->mmap_sem);
}

static inline void unlock_out_mmap(void)
{
	unlock_kernel();
	up(&current->mm->mmap_sem);
}
 
/* different request_region */
#define request_region_check compat_request_region
void *compat_request_region (unsigned long start, unsigned long n, const char *name);
 
#define __exit
#define __init
#define virt_to_page(va) MAP_NR(va)

/* 2.2 has no cpu_number_map on UP */
#ifdef CONFIG_SMP
	#define op_cpu_id() cpu_number_map[smp_processor_id()]
#else
	#define op_cpu_id() smp_processor_id()
#endif /* CONFIG_SMP */

/* provide a working smp_call_function */
#if !defined(CONFIG_SMP)

	#undef smp_call_function
	static int inline smp_call_function (void (*func) (void *info), void *info,
					     int retry, int wait)
	{
		return 0;
	}

#endif /* !CONFIG_SMP */

#if V_BEFORE(2,2,18)

	/* 2.2.18 introduced module_init */
	/* Not sure what version aliases were introduced in, but certainly in 2.91.66.  */
	#ifdef MODULE
	  #if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 91)
	    #define module_init(x)	int init_module(void) __attribute__((alias(#x)));
	    #define module_exit(x)	void cleanup_module(void) __attribute__((alias(#x)));
	  #else
	    #define module_init(x)	int init_module(void) { return x(); }
	    #define module_exit(x)	void cleanup_module(void) { x(); }
	  #endif
	#else
	  #define module_init(x)
	  #define module_exit(x)
	#endif

	/* 2.2.18 introduced vmalloc_32 */
	#define vmalloc_32 vmalloc

	/* 2.2.18 add doubled linked list wait_queue and mutex */
	#define DECLARE_WAIT_QUEUE_HEAD(q) struct wait_queue *q = NULL 
	#define DECLARE_MUTEX(foo)	struct semaphore foo = MUTEX

	/* 2.2.18 add THIS_MODULE */
	#define THIS_MODULE (&__this_module)

#endif /* V_BEFORE(2,2,18) */

/* 2.2.18 introduced the rtc lock */
#ifdef RTC_LOCK
	#define lock_rtc(f) spin_lock_irqsave(&rtc_lock, f)
	#define unlock_rtc(f) spin_unlock_irqrestore(&rtc_lock, f)
#else
	#define lock_rtc(f) do { save_flags(f); cli(); } while (0)
	#define unlock_rtc(f) restore_flags(f)
#endif /* RTC_LOCK */
 
#if V_AT_LEAST(2,2,20)
	#define PTRACE_OFF(t) ((t)->ptrace &= ~PT_DTRACE)
#else
	#define PTRACE_OFF(t) ((t)->flags &= ~PF_DTRACE)
#endif

#endif /* COMPAT22_H */
