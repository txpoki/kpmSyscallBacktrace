/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Common definitions shared across modules
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <compiler.h>
#include <kpmodule.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <asm/current.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/stacktrace.h>
#include <linux/mm_types.h>
#include <linux/kernel.h>
#include <asm/ptrace.h>
#include <asm/processor.h>

#ifndef PSR_MODE32_BIT
#define PSR_MODE32_BIT 0x00000010
#endif

#define MAX_STACK_DEPTH 32

// Forward declarations
struct rw_semaphore;
struct pid_namespace;
struct file;

// Function pointer types
typedef void (*save_stack_trace_user_t)(struct stack_trace *trace);
typedef void (*get_task_comm_t)(char *buf, size_t buf_size, struct task_struct *tsk);
typedef pid_t (*task_pid_nr_ns_t)(struct task_struct *task, enum pid_type type, struct pid_namespace *ns);
typedef int (*get_cmdline_t)(struct task_struct *task, char *buffer, int buflen);
typedef long (*strncpy_from_user_t)(char *dst, const char __user *src, long count);
typedef unsigned long (*arch_copy_from_user_t)(void *to, const void __user *from, unsigned long n);
typedef void (*print_vma_addr_t)(char *prefix, unsigned long ip);
typedef struct vm_area_struct *(*find_vma_t)(struct mm_struct *mm, unsigned long addr);
typedef char *(*file_path_t)(struct file *, char *, int);
typedef int (*down_read_trylock_t)(struct rw_semaphore *sem);
typedef void (*up_read_t)(struct rw_semaphore *sem);
typedef void (*free_page_t)(unsigned long addr, unsigned int order);
typedef unsigned long (*get_free_page_t)(unsigned int gfp_mask, int order);
typedef int (*snprintf_t)(char *buf, size_t size, const char *fmt, ...);

// VMA offset configuration
struct vma_offsets_t {
    int vm_start;
    int vm_end;
    int vm_file;
    int vm_prev;
};

// ARM32 stack frame structure
struct stack_frame_32 {
    u32 next_fp;
    u32 ret_addr;
};

// Helper function
static inline long IS_ERR(const void *ptr)
{
    return (long)ptr >= (unsigned long)-4095;
}

#endif /* _COMMON_H_ */
