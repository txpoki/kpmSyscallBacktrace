/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Hardware Breakpoint Implementation for ARM64
 * Using kernel's hardware breakpoint API via kallsyms
 */

#include <compiler.h>
#include <kpmodule.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include "hw_breakpoint.h"
#include "process_info.h"
#include "stack_unwind.h"

// Forward declarations for structures we'll use as opaque pointers
struct perf_event;
struct perf_event_attr;
struct pt_regs;
struct task_struct;
struct pid;
struct perf_sample_data;  // Add this for correct handler signature

// ARM64 debug register manipulation functions
// These allow us to temporarily disable/enable breakpoints at hardware level

// Function pointer types for debug register access
typedef void (*write_wb_reg_t)(int n, u64 val);
typedef u64 (*read_wb_reg_t)(int n);

// Global function pointers for debug register access
static write_wb_reg_t g_aarch64_insn_write_DBGBCR = NULL;
static write_wb_reg_t g_aarch64_insn_write_DBGBVR = NULL;
static read_wb_reg_t g_aarch64_insn_read_DBGBCR = NULL;
static read_wb_reg_t g_aarch64_insn_read_DBGBVR = NULL;

// Track which hardware slot each breakpoint uses
static int bp_hw_slot[MAX_HW_BREAKPOINTS];  // -1 = not assigned

// Temporarily disable breakpoint at hardware level
static void toggle_bp_hw_disable(int slot)
{
    if (slot < 0 || slot >= 16) return;  // ARM64 supports up to 16 HW breakpoints
    
    if (g_aarch64_insn_write_DBGBCR) {
        // Read current control register
        u64 bcr = 0;
        if (g_aarch64_insn_read_DBGBCR) {
            bcr = g_aarch64_insn_read_DBGBCR(slot);
        }
        
        // Clear enable bit (bit 0)
        bcr &= ~0x1ULL;
        
        // Write back
        g_aarch64_insn_write_DBGBCR(slot, bcr);
    }
}

// Re-enable breakpoint at hardware level
static void toggle_bp_hw_enable(int slot)
{
    if (slot < 0 || slot >= 16) return;
    
    if (g_aarch64_insn_write_DBGBCR) {
        // Read current control register
        u64 bcr = 0;
        if (g_aarch64_insn_read_DBGBCR) {
            bcr = g_aarch64_insn_read_DBGBCR(slot);
        }
        
        // Set enable bit (bit 0)
        bcr |= 0x1ULL;
        
        // Write back
        g_aarch64_insn_write_DBGBCR(slot, bcr);
    }
}

// Function pointer types for hardware breakpoint API
typedef struct perf_event *(*register_wide_hw_breakpoint_t)(
    struct perf_event_attr *attr,
    void *triggered,
    void *context);

typedef struct perf_event *(*register_user_hw_breakpoint_t)(
    struct perf_event_attr *attr,
    void *triggered,
    void *context,
    struct task_struct *tsk);

typedef void (*unregister_wide_hw_breakpoint_t)(struct perf_event *bp);

typedef void (*unregister_hw_breakpoint_t)(struct perf_event *bp);

typedef int (*modify_user_hw_breakpoint_t)(
    struct perf_event *bp,
    struct perf_event_attr *attr);

// PID lookup function types
typedef struct pid *(*find_vpid_t)(int nr);
typedef struct task_struct *(*pid_task_t)(struct pid *pid, int type);

// Global function pointers
static register_wide_hw_breakpoint_t g_register_wide_hw_breakpoint = NULL;
static register_user_hw_breakpoint_t g_register_user_hw_breakpoint = NULL;
static unregister_wide_hw_breakpoint_t g_unregister_wide_hw_breakpoint = NULL;
static unregister_hw_breakpoint_t g_unregister_hw_breakpoint = NULL;
static modify_user_hw_breakpoint_t g_modify_user_hw_breakpoint = NULL;
static find_vpid_t g_find_vpid = NULL;
static pid_task_t g_pid_task = NULL;

// Breakpoint storage
static struct hw_breakpoint breakpoints[MAX_HW_BREAKPOINTS];
static struct perf_event *bp_events[MAX_HW_BREAKPOINTS];
static struct task_struct *bp_tasks[MAX_HW_BREAKPOINTS];  // Track target task for each breakpoint
static int bp_verbose_mode = 0;  // Verbose logging mode (disabled by default for safety)

// Track temporary state for move-to-next-instruction mechanism
static int bp_at_next_instruction[MAX_HW_BREAKPOINTS];  // 1 = currently at next instruction
static unsigned long bp_original_addr[MAX_HW_BREAKPOINTS];  // Original breakpoint address
static unsigned long bp_next_addr[MAX_HW_BREAKPOINTS];  // Next instruction address

// perf_event_attr structure (minimal definition to avoid header conflicts)
struct perf_event_attr_minimal {
    unsigned int type;
    unsigned int size;
    unsigned long long config;
    unsigned long long sample_period;
    unsigned long long sample_type;
    unsigned long long read_format;
    unsigned long long disabled       : 1,
                       inherit        : 1,
                       pinned         : 1,
                       exclusive      : 1,
                       exclude_user   : 1,
                       exclude_kernel : 1,
                       exclude_hv     : 1,
                       exclude_idle   : 1,
                       mmap           : 1,
                       comm           : 1,
                       freq           : 1,
                       inherit_stat   : 1,
                       enable_on_exec : 1,
                       task           : 1,
                       watermark      : 1,
                       precise_ip     : 2,
                       mmap_data      : 1,
                       sample_id_all  : 1,
                       exclude_host   : 1,
                       exclude_guest  : 1,
                       exclude_callchain_kernel : 1,
                       exclude_callchain_user   : 1,
                       mmap2          : 1,
                       comm_exec      : 1,
                       use_clockid    : 1,
                       context_switch : 1,
                       write_backward : 1,
                       namespaces     : 1,
                       __reserved_1   : 35;
    unsigned int wakeup_events;
    unsigned int bp_type;
    unsigned long long bp_addr;
    unsigned long long bp_len;
    // ... more fields exist but we only need these
};

// Constants from perf_event.h
#define PERF_TYPE_BREAKPOINT 5

// Hardware breakpoint type constants
#define HW_BREAKPOINT_EMPTY     0
#define HW_BREAKPOINT_R         1
#define HW_BREAKPOINT_W         2
#define HW_BREAKPOINT_RW        (HW_BREAKPOINT_R | HW_BREAKPOINT_W)
#define HW_BREAKPOINT_X         4

// Hardware breakpoint length constants
#define HW_BREAKPOINT_LEN_1     1
#define HW_BREAKPOINT_LEN_2     2
#define HW_BREAKPOINT_LEN_4     4
#define HW_BREAKPOINT_LEN_8     8

// PID type constant
#define PIDTYPE_PID 0

// Breakpoint handler callback
// CRITICAL: This runs in INTERRUPT CONTEXT - must be extremely minimal
// IMPORTANT: Signature MUST match perf_overflow_handler_t exactly
//
// Strategy: Move-to-next-instruction mechanism
// - First hit (at original address): Move breakpoint to next instruction
// - Second hit (at next instruction): Move breakpoint back to original address
// - This allows the instruction to execute and breakpoint to remain active
static void hw_bp_handler(struct perf_event *bp, 
                          struct perf_sample_data *data,
                          struct pt_regs *regs)
{
    int i;
    struct perf_event_attr_minimal attr;
    int result;
    
    // Find which breakpoint was hit
    for (i = 0; i < MAX_HW_BREAKPOINTS; i++) {
        if (bp_events[i] == bp && breakpoints[i].enabled) {
            
            if (!bp_at_next_instruction[i]) {
                // FIRST HIT: At original address
                breakpoints[i].hit_count++;
                
                pr_info("HW_BP[%d]: Hit at 0x%lx (original), count:%d\n",
                        i, breakpoints[i].addr, breakpoints[i].hit_count);
                
                // Move breakpoint to next instruction
                // This allows current instruction to execute
                if (g_modify_user_hw_breakpoint && bp_events[i]) {
                    // Calculate next instruction address (ARM64: +4 bytes)
                    bp_next_addr[i] = regs->pc + 4;
                    
                    // Prepare new attributes
                    memset(&attr, 0, sizeof(attr));
                    attr.type = PERF_TYPE_BREAKPOINT;
                    attr.size = sizeof(attr);
                    attr.bp_addr = bp_next_addr[i];
                    attr.bp_type = HW_BREAKPOINT_X;  // Execution breakpoint
                    attr.bp_len = HW_BREAKPOINT_LEN_4;
                    attr.disabled = 0;
                    attr.sample_period = 1;
                    attr.freq = 0;
                    attr.exclude_kernel = 0;
                    attr.exclude_user = 0;
                    attr.exclude_hv = 0;
                    
                    // Modify breakpoint to next instruction
                    result = g_modify_user_hw_breakpoint(bp_events[i], (struct perf_event_attr *)&attr);
                    
                    if (result == 0) {
                        bp_at_next_instruction[i] = 1;
                        pr_info("HW_BP[%d]: Moved to next instruction 0x%lx\n", i, bp_next_addr[i]);
                    } else {
                        pr_err("HW_BP[%d]: Failed to move to next instruction: %d\n", i, result);
                        // Fallback: disable breakpoint
                        if (g_unregister_wide_hw_breakpoint) {
                            if (bp_tasks[i] && g_unregister_hw_breakpoint) {
                                g_unregister_hw_breakpoint(bp_events[i]);
                            } else {
                                g_unregister_wide_hw_breakpoint(bp_events[i]);
                            }
                            breakpoints[i].enabled = 0;
                            bp_events[i] = NULL;
                            bp_tasks[i] = NULL;
                        }
                    }
                } else {
                    // modify_user_hw_breakpoint not available, use one-shot mode
                    pr_warn("HW_BP[%d]: modify_user_hw_breakpoint not available, disabling\n", i);
                    if (g_unregister_wide_hw_breakpoint && bp_events[i]) {
                        if (bp_tasks[i] && g_unregister_hw_breakpoint) {
                            g_unregister_hw_breakpoint(bp_events[i]);
                        } else {
                            g_unregister_wide_hw_breakpoint(bp_events[i]);
                        }
                        breakpoints[i].enabled = 0;
                        bp_events[i] = NULL;
                        bp_tasks[i] = NULL;
                    }
                }
                
            } else {
                // SECOND HIT: At next instruction
                pr_info("HW_BP[%d]: Hit at 0x%lx (next instruction)\n", i, regs->pc);
                
                // Move breakpoint back to original address
                if (g_modify_user_hw_breakpoint && bp_events[i]) {
                    // Prepare original attributes
                    memset(&attr, 0, sizeof(attr));
                    attr.type = PERF_TYPE_BREAKPOINT;
                    attr.size = sizeof(attr);
                    attr.bp_addr = bp_original_addr[i];
                    attr.bp_type = HW_BREAKPOINT_X;
                    attr.bp_len = HW_BREAKPOINT_LEN_4;
                    attr.disabled = 0;
                    attr.sample_period = 1;
                    attr.freq = 0;
                    attr.exclude_kernel = 0;
                    attr.exclude_user = 0;
                    attr.exclude_hv = 0;
                    
                    // Modify breakpoint back to original
                    result = g_modify_user_hw_breakpoint(bp_events[i], (struct perf_event_attr *)&attr);
                    
                    if (result == 0) {
                        bp_at_next_instruction[i] = 0;
                        pr_info("HW_BP[%d]: Moved back to original 0x%lx\n", i, bp_original_addr[i]);
                    } else {
                        pr_err("HW_BP[%d]: Failed to move back to original: %d\n", i, result);
                    }
                }
            }
            
            break;
        }
    }
}

int hw_breakpoint_init(void)
{
    int i;
    
    // Initialize breakpoint array
    for (i = 0; i < MAX_HW_BREAKPOINTS; i++) {
        breakpoints[i].addr = 0;
        breakpoints[i].type = HW_BP_TYPE_EXEC;
        breakpoints[i].size = HW_BP_SIZE_4;
        breakpoints[i].enabled = 0;
        breakpoints[i].hit_count = 0;
        breakpoints[i].description[0] = '\0';
        bp_events[i] = NULL;
        bp_tasks[i] = NULL;
        bp_hw_slot[i] = -1;
        bp_at_next_instruction[i] = 0;
        bp_original_addr[i] = 0;
        bp_next_addr[i] = 0;
    }
    
    // Resolve hardware breakpoint API functions
    g_register_wide_hw_breakpoint = (register_wide_hw_breakpoint_t)
        kallsyms_lookup_name("register_wide_hw_breakpoint");
    g_register_user_hw_breakpoint = (register_user_hw_breakpoint_t)
        kallsyms_lookup_name("register_user_hw_breakpoint");
    g_unregister_wide_hw_breakpoint = (unregister_wide_hw_breakpoint_t)
        kallsyms_lookup_name("unregister_wide_hw_breakpoint");
    g_unregister_hw_breakpoint = (unregister_hw_breakpoint_t)
        kallsyms_lookup_name("unregister_hw_breakpoint");
    g_modify_user_hw_breakpoint = (modify_user_hw_breakpoint_t)
        kallsyms_lookup_name("modify_user_hw_breakpoint");
    
    // Resolve PID lookup functions
    g_find_vpid = (find_vpid_t)kallsyms_lookup_name("find_vpid");
    g_pid_task = (pid_task_t)kallsyms_lookup_name("pid_task");
    
    if (!g_register_wide_hw_breakpoint || !g_unregister_wide_hw_breakpoint) {
        pr_err("Failed to resolve hardware breakpoint API functions\n");
        pr_err("  register_wide_hw_breakpoint: %p\n", g_register_wide_hw_breakpoint);
        pr_err("  unregister_wide_hw_breakpoint: %p\n", g_unregister_wide_hw_breakpoint);
        return -1;
    }
    
    pr_info("Hardware breakpoint subsystem initialized (%d slots)\n", MAX_HW_BREAKPOINTS);
    pr_info("  register_wide_hw_breakpoint: %p\n", g_register_wide_hw_breakpoint);
    pr_info("  register_user_hw_breakpoint: %p\n", g_register_user_hw_breakpoint);
    pr_info("  unregister_wide_hw_breakpoint: %p\n", g_unregister_wide_hw_breakpoint);
    pr_info("  unregister_hw_breakpoint: %p\n", g_unregister_hw_breakpoint);
    pr_info("  modify_user_hw_breakpoint: %p\n", g_modify_user_hw_breakpoint);
    pr_info("  find_vpid: %p\n", g_find_vpid);
    pr_info("  pid_task: %p\n", g_pid_task);
    
    if (!g_find_vpid || !g_pid_task) {
        pr_warn("PID lookup functions not available, per-process breakpoints disabled\n");
    }
    
    if (!g_modify_user_hw_breakpoint) {
        pr_warn("modify_user_hw_breakpoint not available\n");
        pr_warn("Using one-shot breakpoint mode (breakpoint disables after first hit)\n");
    } else {
        pr_info("modify_user_hw_breakpoint available, using move-to-next-instruction mechanism\n");
    }
    
    return 0;
}

int hw_breakpoint_set(unsigned long addr, int type, int size, const char *desc)
{
    return hw_breakpoint_set_for_pid(addr, type, size, 0, desc);
}

int hw_breakpoint_set_for_pid(unsigned long addr, int type, int size, int pid, const char *desc)
{
    struct perf_event_attr_minimal attr;
    struct perf_event *bp;
    struct task_struct *target_task = NULL;
    int i;
    unsigned int bp_type, bp_len;
    
    if (!g_register_wide_hw_breakpoint && !g_register_user_hw_breakpoint) {
        pr_err("Hardware breakpoint API not available\n");
        return -ENOSYS;
    }
    
    // Validate parameters
    if (type < HW_BP_TYPE_EXEC || type > HW_BP_TYPE_RW) {
        pr_err("Invalid breakpoint type: %d\n", type);
        return -EINVAL;
    }
    
    if (size < HW_BP_SIZE_1 || size > HW_BP_SIZE_8) {
        pr_err("Invalid breakpoint size: %d\n", size);
        return -EINVAL;
    }
    
    if (addr == 0) {
        pr_err("Invalid breakpoint address: 0x%lx\n", addr);
        return -EINVAL;
    }
    
    // If PID is specified, find the task
    if (pid > 0) {
        if (!g_register_user_hw_breakpoint) {
            pr_err("register_user_hw_breakpoint not available\n");
            return -ENOSYS;
        }
        
        if (!g_find_vpid || !g_pid_task) {
            pr_err("PID lookup functions not available\n");
            return -ENOSYS;
        }
        
        // Find task by PID using resolved functions
        struct pid *pid_struct = g_find_vpid(pid);
        if (!pid_struct) {
            pr_err("Process with PID %d not found (find_vpid failed)\n", pid);
            return -ESRCH;
        }
        
        target_task = g_pid_task(pid_struct, PIDTYPE_PID);
        if (!target_task) {
            pr_err("Process with PID %d not found (pid_task failed)\n", pid);
            return -ESRCH;
        }
        
        pr_info("Setting breakpoint for process PID=%d\n", pid);
    }
    
    // Find free slot
    for (i = 0; i < MAX_HW_BREAKPOINTS; i++) {
        if (!breakpoints[i].enabled) {
            break;
        }
    }
    
    if (i >= MAX_HW_BREAKPOINTS) {
        pr_err("No free hardware breakpoint slots\n");
        return -ENOMEM;
    }
    
    // Convert type
    switch (type) {
        case HW_BP_TYPE_EXEC:
            bp_type = HW_BREAKPOINT_X;
            break;
        case HW_BP_TYPE_WRITE:
            bp_type = HW_BREAKPOINT_W;
            break;
        case HW_BP_TYPE_READ:
            bp_type = HW_BREAKPOINT_R;
            break;
        case HW_BP_TYPE_RW:
            bp_type = HW_BREAKPOINT_RW;
            break;
        default:
            pr_err("Invalid breakpoint type: %d\n", type);
            return -EINVAL;
    }
    
    // Convert size
    switch (size) {
        case HW_BP_SIZE_1:
            bp_len = HW_BREAKPOINT_LEN_1;
            break;
        case HW_BP_SIZE_2:
            bp_len = HW_BREAKPOINT_LEN_2;
            break;
        case HW_BP_SIZE_4:
            bp_len = HW_BREAKPOINT_LEN_4;
            break;
        case HW_BP_SIZE_8:
            bp_len = HW_BREAKPOINT_LEN_8;
            break;
        default:
            pr_err("Invalid breakpoint size: %d\n", size);
            return -EINVAL;
    }
    
    // Setup perf_event attributes
    memset(&attr, 0, sizeof(attr));
    attr.type = PERF_TYPE_BREAKPOINT;
    attr.size = sizeof(attr);
    attr.bp_addr = addr;
    attr.bp_type = bp_type;
    attr.bp_len = bp_len;
    
    // CRITICAL: Configure behavior for sampling breakpoints
    // disabled = 0: breakpoint is active immediately
    attr.disabled = 0;
    
    // For execution breakpoints, we need special handling to avoid infinite triggers
    // Option 1: Use sample_period > 1 to skip some hits
    // Option 2: Disable after first hit (one-shot mode)
    // Option 3: Use single-step mechanism (complex)
    //
    // We'll use a hybrid approach:
    // - Set sample_period to a reasonable value to avoid infinite loops
    // - Let the handler skip the instruction for execution breakpoints
    if (type == HW_BP_TYPE_EXEC) {
        // For execution breakpoints: sample every 1 hit but handler will skip instruction
        attr.sample_period = 1;
    } else {
        // For data breakpoints: sample every hit (they don't have the re-trigger problem)
        attr.sample_period = 1;
    }
    
    // Don't exclude any context - we want to catch all hits
    attr.exclude_kernel = 0;
    attr.exclude_user = 0;
    attr.exclude_hv = 0;
    
    // Don't use frequency-based sampling, use period-based
    attr.freq = 0;
    
    // Register breakpoint (system-wide or per-process)
    if (target_task) {
        // Per-process breakpoint
        bp = g_register_user_hw_breakpoint((struct perf_event_attr *)&attr, 
                                           (void *)hw_bp_handler, NULL, target_task);
    } else {
        // System-wide breakpoint
        bp = g_register_wide_hw_breakpoint((struct perf_event_attr *)&attr, 
                                           (void *)hw_bp_handler, NULL);
    }
    
    // Check for errors (IS_ERR macro equivalent)
    if ((unsigned long)bp >= (unsigned long)-4095) {
        long err = (long)bp;
        pr_err("Failed to register hardware breakpoint: %ld\n", err);
        return (int)err;
    }
    
    // Store breakpoint info
    breakpoints[i].addr = addr;
    breakpoints[i].type = type;
    breakpoints[i].size = size;
    breakpoints[i].enabled = 1;
    breakpoints[i].hit_count = 0;
    bp_events[i] = bp;
    bp_tasks[i] = target_task;
    bp_at_next_instruction[i] = 0;
    bp_original_addr[i] = addr;  // Save original address for move-to-next mechanism
    bp_next_addr[i] = 0;
    
    // Try to extract hardware slot from perf_event
    bp_hw_slot[i] = i;  // Assume slot matches our index
    
    if (desc) {
        strncpy(breakpoints[i].description, desc, sizeof(breakpoints[i].description) - 1);
        breakpoints[i].description[sizeof(breakpoints[i].description) - 1] = '\0';
    } else {
        breakpoints[i].description[0] = '\0';
    }
    
    if (target_task) {
        char task_name[256] = {0};
        get_process_cmdline(target_task, task_name, sizeof(task_name));
        pr_info("Hardware breakpoint[%d] set at 0x%lx for PID=%d [%s] (type=%d, size=%d)\n", 
                i, addr, pid, task_name, type, size);
    } else {
        pr_info("Hardware breakpoint[%d] set at 0x%lx (system-wide, type=%d, size=%d)\n", 
                i, addr, type, size);
    }
    
    if (g_modify_user_hw_breakpoint) {
        pr_info("Hardware breakpoint[%d]: Using move-to-next-instruction mechanism\n", i);
    } else {
        pr_info("Hardware breakpoint[%d]: Using one-shot mode (will disable after first hit)\n", i);
    }
    
    return i;
}

int hw_breakpoint_clear(int index)
{
    if (index < 0 || index >= MAX_HW_BREAKPOINTS) {
        return -EINVAL;
    }
    
    if (!breakpoints[index].enabled) {
        return -ENOENT;
    }
    
    // Unregister breakpoint
    if (bp_events[index]) {
        if (bp_tasks[index] && g_unregister_hw_breakpoint) {
            // Per-process breakpoint
            g_unregister_hw_breakpoint(bp_events[index]);
        } else if (g_unregister_wide_hw_breakpoint) {
            // System-wide breakpoint
            g_unregister_wide_hw_breakpoint(bp_events[index]);
        }
        bp_events[index] = NULL;
        bp_tasks[index] = NULL;
    }
    
    pr_info("Hardware breakpoint[%d] cleared (was at 0x%lx, hit %d times)\n",
            index, breakpoints[index].addr, breakpoints[index].hit_count);
    
    // Clear slot
    breakpoints[index].enabled = 0;
    breakpoints[index].addr = 0;
    breakpoints[index].hit_count = 0;
    
    return 0;
}

void hw_breakpoint_clear_all(void)
{
    int i;
    
    for (i = 0; i < MAX_HW_BREAKPOINTS; i++) {
        if (breakpoints[i].enabled) {
            hw_breakpoint_clear(i);
        }
    }
    
    pr_info("All hardware breakpoints cleared\n");
}

struct hw_breakpoint *hw_breakpoint_get(int index)
{
    if (index < 0 || index >= MAX_HW_BREAKPOINTS) {
        return NULL;
    }
    
    return &breakpoints[index];
}

int hw_breakpoint_get_count(void)
{
    int i, count = 0;
    
    for (i = 0; i < MAX_HW_BREAKPOINTS; i++) {
        if (breakpoints[i].enabled) {
            count++;
        }
    }
    
    return count;
}

void hw_breakpoint_set_verbose(int verbose)
{
    bp_verbose_mode = verbose;
    if (verbose) {
        pr_warn("Verbose mode requested but not implemented (workqueue not available in KernelPatch)\n");
        pr_warn("Only minimal logging is available: HW_BP[N]: Hit at 0xADDR, count:N\n");
    } else {
        pr_info("Hardware breakpoint verbose mode: disabled (minimal logging)\n");
    }
}

int hw_breakpoint_get_verbose(void)
{
    return 0;  // Always return 0 since verbose mode is not implemented
}
