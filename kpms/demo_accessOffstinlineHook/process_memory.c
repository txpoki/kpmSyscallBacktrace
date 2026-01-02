/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Process Memory Access Implementation
 */

#include <compiler.h>
#include <kpmodule.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include "process_memory.h"

// Forward declarations
struct task_struct;
struct mm_struct;
struct pid;

// Function pointer types
typedef struct pid *(*find_vpid_t)(int nr);
typedef struct task_struct *(*pid_task_t)(struct pid *pid, int type);
typedef int (*access_process_vm_t)(struct task_struct *tsk, unsigned long addr,
                                   void *buf, int len, unsigned int gup_flags);
typedef struct mm_struct *(*get_task_mm_t)(struct task_struct *task);
typedef void (*mmput_t)(struct mm_struct *mm);

// Global function pointers
static find_vpid_t g_find_vpid = NULL;
static pid_task_t g_pid_task = NULL;
static access_process_vm_t g_access_process_vm = NULL;
static get_task_mm_t g_get_task_mm = NULL;
static mmput_t g_mmput = NULL;

// Constants
#define PIDTYPE_PID 0
#define FOLL_FORCE 0x10  // Force access even if page is not writable

int process_memory_init(void)
{
    // Resolve required functions
    g_find_vpid = (find_vpid_t)kallsyms_lookup_name("find_vpid");
    g_pid_task = (pid_task_t)kallsyms_lookup_name("pid_task");
    g_access_process_vm = (access_process_vm_t)kallsyms_lookup_name("access_process_vm");
    g_get_task_mm = (get_task_mm_t)kallsyms_lookup_name("get_task_mm");
    g_mmput = (mmput_t)kallsyms_lookup_name("mmput");
    
    if (!g_find_vpid || !g_pid_task) {
        pr_err("Failed to resolve PID lookup functions\n");
        pr_err("  find_vpid: %p\n", g_find_vpid);
        pr_err("  pid_task: %p\n", g_pid_task);
        return -1;
    }
    
    if (!g_access_process_vm) {
        pr_err("Failed to resolve access_process_vm\n");
        return -1;
    }
    
    pr_info("Process memory subsystem initialized\n");
    pr_info("  find_vpid: %p\n", g_find_vpid);
    pr_info("  pid_task: %p\n", g_pid_task);
    pr_info("  access_process_vm: %p\n", g_access_process_vm);
    pr_info("  get_task_mm: %p\n", g_get_task_mm);
    pr_info("  mmput: %p\n", g_mmput);
    
    return 0;
}

int process_memory_read(int pid, unsigned long addr, void *buf, size_t size)
{
    struct task_struct *task;
    struct pid *pid_struct;
    struct mm_struct *mm;
    int ret;
    
    if (!g_find_vpid || !g_pid_task || !g_access_process_vm) {
        pr_err("Process memory functions not available\n");
        return -ENOSYS;
    }
    
    if (!buf || size == 0 || size > MAX_READ_SIZE) {
        pr_err("Invalid parameters: buf=%p, size=%zu\n", buf, size);
        return -EINVAL;
    }
    
    // Find the task
    pid_struct = g_find_vpid(pid);
    if (!pid_struct) {
        pr_err("Process with PID %d not found (find_vpid failed)\n", pid);
        return -ESRCH;
    }
    
    task = g_pid_task(pid_struct, PIDTYPE_PID);
    if (!task) {
        pr_err("Process with PID %d not found (pid_task failed)\n", pid);
        return -ESRCH;
    }
    
    // Get the mm_struct if available
    if (g_get_task_mm) {
        mm = g_get_task_mm(task);
        if (!mm) {
            pr_err("Failed to get mm_struct for PID %d\n", pid);
            return -EINVAL;
        }
    }
    
    // Read from process memory
    ret = g_access_process_vm(task, addr, buf, size, 0);
    
    // Release mm_struct if we got it
    if (g_get_task_mm && g_mmput && mm) {
        g_mmput(mm);
    }
    
    if (ret < 0) {
        pr_err("Failed to read memory from PID %d at 0x%lx: %d\n", pid, addr, ret);
        return ret;
    }
    
    if (ret == 0) {
        pr_warn("Read 0 bytes from PID %d at 0x%lx (invalid address?)\n", pid, addr);
        return -EFAULT;
    }
    
    pr_info("Read %d bytes from PID %d at 0x%lx\n", ret, pid, addr);
    return ret;
}

int process_memory_read_hex(int pid, unsigned long addr, char *out, size_t out_size, size_t read_size)
{
    unsigned char buf[MAX_READ_SIZE];
    int ret, i;
    size_t pos = 0;
    
    if (!out || out_size == 0) {
        return -EINVAL;
    }
    
    if (read_size > MAX_READ_SIZE) {
        read_size = MAX_READ_SIZE;
    }
    
    // Read memory
    ret = process_memory_read(pid, addr, buf, read_size);
    if (ret < 0) {
        return ret;
    }
    
    // Format as hex string
    for (i = 0; i < ret && pos < out_size - 3; i++) {
        int written = snprintf(out + pos, out_size - pos, "%02x ", buf[i]);
        if (written < 0) {
            break;
        }
        pos += written;
    }
    
    // Remove trailing space
    if (pos > 0 && out[pos - 1] == ' ') {
        out[pos - 1] = '\0';
    } else {
        out[pos] = '\0';
    }
    
    return ret;
}
