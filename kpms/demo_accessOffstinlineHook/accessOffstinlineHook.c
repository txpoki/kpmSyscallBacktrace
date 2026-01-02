/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <compiler.h>
#include <hook.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include "common.h"
#include "stack_unwind.h"
#include "process_info.h"
#include "hw_breakpoint.h"
#include "process_memory.h"

KPM_NAME("kpm-inline-access");
KPM_VERSION("10.3.0");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("bmax121 & User");
KPM_DESCRIPTION("Inline Hook with filtering, supercall control, hardware breakpoints, and memory access");

// Filter configuration
#define MAX_FILTERS 16
#define MAX_NAME_LEN 256

struct filter_entry {
    char name[MAX_NAME_LEN];  // Package name or process name
    int pid;                   // PID (0 = not used)
    int active;                // Is this filter active?
};

// Module state
static struct {
    int hook_enabled;
    int access_hook_count;
    int openat_hook_count;
    int kill_hook_count;
    
    // Filter settings
    int filter_mode;           // 0=whitelist, 1=blacklist
    int hook_access_enabled;   // Enable access hook
    int hook_openat_enabled;   // Enable openat hook
    int hook_kill_enabled;     // Enable kill hook
    
    // Filters
    struct filter_entry filters[MAX_FILTERS];
    int filter_count;
} module_state = {
    .hook_enabled = 0,         // Default: disabled
    .access_hook_count = 0,
    .openat_hook_count = 0,
    .kill_hook_count = 0,
    .filter_mode = 0,          // Default: whitelist (no filtering)
    .hook_access_enabled = 0,  // Default: disabled
    .hook_openat_enabled = 0,  // Default: disabled
    .hook_kill_enabled = 0,    // Default: disabled
    .filter_count = 0
};

// Global variables for this module
static strncpy_from_user_t g_strncpy_from_user = NULL;
static print_vma_addr_t g_print_vma_addr = NULL;
static arch_copy_to_user_t g_arch_copy_to_user = NULL;
static void *g_do_faccessat_addr = NULL;
static void *g_do_sys_openat2_addr = NULL;  // openat hook target
static void *g_sys_kill_addr = NULL;         // kill hook target

/**
 * Check if process should be filtered
 * Returns: 1 = should hook, 0 = should skip
 */
static int should_hook_process(struct task_struct *task)
{
    char pkg_name[MAX_NAME_LEN];
    int pid = get_process_id(task);
    int i;
    
    // If no filters, hook everything
    if (module_state.filter_count == 0) {
        return 1;
    }
    
    get_process_cmdline(task, pkg_name, sizeof(pkg_name));
    
    // Check filters
    for (i = 0; i < module_state.filter_count; i++) {
        if (!module_state.filters[i].active) continue;
        
        // Check PID match
        if (module_state.filters[i].pid > 0 && module_state.filters[i].pid == pid) {
            return (module_state.filter_mode == 0) ? 1 : 0;  // whitelist:hook, blacklist:skip
        }
        
        // Check name match
        if (module_state.filters[i].name[0] != '\0' && 
            strstr(pkg_name, module_state.filters[i].name) != NULL) {
            return (module_state.filter_mode == 0) ? 1 : 0;  // whitelist:hook, blacklist:skip
        }
    }
    
    // No match found
    return (module_state.filter_mode == 0) ? 0 : 1;  // whitelist:skip, blacklist:hook
}

void before_do_faccessat(hook_fargs3_t *args, void *udata)
{
    struct task_struct *task = current;
    
    // Check if hook is enabled
    if (!module_state.hook_enabled || !module_state.hook_access_enabled) {
        return;
    }
    
    // Check filter
    if (!should_hook_process(task)) {
        return;
    }

    module_state.access_hook_count++;

    const char __user *filename = (const char __user *)args->arg1;
    int mode = (int)args->arg2;

    char path_buf[256] = {0};
    char pkg_name[256] = {0};

    if (g_strncpy_from_user) {
        long ret = g_strncpy_from_user(path_buf, filename, sizeof(path_buf) - 1);
        if (ret < 0) strncpy(path_buf, "<read_error>", sizeof(path_buf));
    } else {
        strncpy(path_buf, "<symbol_missing>", sizeof(path_buf));
    }

    get_process_cmdline(task, pkg_name, sizeof(pkg_name));

    pr_info("INLINE_ACCESS: [%s] (PID:%d) -> %s [Mode:%d]\n", 
            pkg_name, get_process_id(task), path_buf, mode);

    unwind_user_stack_standard(task);
}

void before_do_sys_openat2(hook_fargs4_t *args, void *udata)
{
    struct task_struct *task = current;
    
    // Check if hook is enabled
    if (!module_state.hook_enabled || !module_state.hook_openat_enabled) {
        return;
    }
    
    // Check filter
    if (!should_hook_process(task)) {
        return;
    }

    module_state.openat_hook_count++;

    int dfd = (int)args->arg0;
    const char __user *filename = (const char __user *)args->arg1;
    // arg2 is struct open_how *how
    // arg3 is size_t size

    char path_buf[256] = {0};
    char pkg_name[256] = {0};

    if (g_strncpy_from_user) {
        long ret = g_strncpy_from_user(path_buf, filename, sizeof(path_buf) - 1);
        if (ret < 0) strncpy(path_buf, "<read_error>", sizeof(path_buf));
    } else {
        strncpy(path_buf, "<symbol_missing>", sizeof(path_buf));
    }

    get_process_cmdline(task, pkg_name, sizeof(pkg_name));

    pr_info("INLINE_OPENAT: [%s] (PID:%d) -> %s [DFD:%d]\n", 
            pkg_name, get_process_id(task), path_buf, dfd);

    unwind_user_stack_standard(task);
}

void before_sys_kill(hook_fargs1_t *args, void *udata)
{
    struct task_struct *task = current;
    
    // Check if hook is enabled
    if (!module_state.hook_enabled || !module_state.hook_kill_enabled) {
        return;
    }
    
    // Check filter
    if (!should_hook_process(task)) {
        return;
    }

    // ARM64: __arm64_sys_kill takes pt_regs as argument
    // Extract syscall arguments from pt_regs
    struct pt_regs *regs = (struct pt_regs *)args->arg0;
    
    // On ARM64, syscall arguments are in regs->regs[0], regs->regs[1], etc.
    pid_t target_pid = (pid_t)regs->regs[0];
    int sig = (int)regs->regs[1];
    
    // Validate arguments - kill() should have reasonable values
    // Skip if this looks like a return value (negative small number) or invalid
    // Valid PIDs: -1 (all processes), 0 (process group), or positive
    // Sometimes the hook is called on return path where regs contain return values
    if (target_pid < -1 || target_pid > 99999) {
        // This might be a return value or corrupted data, skip silently
        return;
    }
    
    if (sig < 0 || sig > 64) {
        // Invalid signal, skip
        return;
    }

    module_state.kill_hook_count++;

    char pkg_name[256] = {0};
    
    get_process_cmdline(task, pkg_name, sizeof(pkg_name));

    pr_info("INLINE_KILL: [%s] (PID:%d) -> kill(PID:%d, SIG:%d)\n", 
            pkg_name, get_process_id(task), target_pid, sig);

    unwind_user_stack_standard(task);
}

/**
 * Supercall control handler
 * Allows userspace to control the module
 * 
 * Note: ctl_args is already in kernel space (copied by KernelPatch)
 *       out_msg is in user space and needs copy_to_user
 */
static long kpm_control(const char *ctl_args, char *__user out_msg, int outlen)
{
    char kernel_out[1024];
    long ret = 0;
    int i;
    
    // ctl_args is already in kernel space, no need to copy
    pr_info("[Control] Received command: %s\n", ctl_args);
    
    // Command: get_status - Get module status
    if (strcmp(ctl_args, "get_status") == 0) {
        int len = snprintf(kernel_out, sizeof(kernel_out),
                 "enabled=%d\n"
                 "access_hook=%d\n"
                 "openat_hook=%d\n"
                 "kill_hook=%d\n"
                 "access_count=%d\n"
                 "openat_count=%d\n"
                 "kill_count=%d\n"
                 "total_hooks=%d\n"
                 "filter_mode=%s\n"
                 "filter_count=%d",
                 module_state.hook_enabled,
                 module_state.hook_access_enabled,
                 module_state.hook_openat_enabled,
                 module_state.hook_kill_enabled,
                 module_state.access_hook_count,
                 module_state.openat_hook_count,
                 module_state.kill_hook_count,
                 module_state.access_hook_count + module_state.openat_hook_count + module_state.kill_hook_count,
                 module_state.filter_mode == 0 ? "whitelist" : "blacklist",
                 module_state.filter_count);
        
        // Add filter list
        for (i = 0; i < module_state.filter_count && len < sizeof(kernel_out) - 100; i++) {
            if (module_state.filters[i].active) {
                if (module_state.filters[i].pid > 0) {
                    len += snprintf(kernel_out + len, sizeof(kernel_out) - len,
                                  "\nfilter[%d]=pid:%d", i, module_state.filters[i].pid);
                } else {
                    len += snprintf(kernel_out + len, sizeof(kernel_out) - len,
                                  "\nfilter[%d]=name:%s", i, module_state.filters[i].name);
                }
            }
        }
    }
    // Command: enable - Enable hooks
    else if (strcmp(ctl_args, "enable") == 0) {
        module_state.hook_enabled = 1;
        snprintf(kernel_out, sizeof(kernel_out), "Hooks enabled");
    }
    // Command: disable - Disable hooks
    else if (strcmp(ctl_args, "disable") == 0) {
        module_state.hook_enabled = 0;
        snprintf(kernel_out, sizeof(kernel_out), "Hooks disabled");
    }
    // Command: enable_access - Enable access hook
    else if (strcmp(ctl_args, "enable_access") == 0) {
        module_state.hook_access_enabled = 1;
        snprintf(kernel_out, sizeof(kernel_out), "Access hook enabled");
    }
    // Command: disable_access - Disable access hook
    else if (strcmp(ctl_args, "disable_access") == 0) {
        module_state.hook_access_enabled = 0;
        snprintf(kernel_out, sizeof(kernel_out), "Access hook disabled");
    }
    // Command: enable_openat - Enable openat hook
    else if (strcmp(ctl_args, "enable_openat") == 0) {
        module_state.hook_openat_enabled = 1;
        snprintf(kernel_out, sizeof(kernel_out), "Openat hook enabled");
    }
    // Command: disable_openat - Disable openat hook
    else if (strcmp(ctl_args, "disable_openat") == 0) {
        module_state.hook_openat_enabled = 0;
        snprintf(kernel_out, sizeof(kernel_out), "Openat hook disabled");
    }
    // Command: enable_kill - Enable kill hook
    else if (strcmp(ctl_args, "enable_kill") == 0) {
        module_state.hook_kill_enabled = 1;
        snprintf(kernel_out, sizeof(kernel_out), "Kill hook enabled");
    }
    // Command: disable_kill - Disable kill hook
    else if (strcmp(ctl_args, "disable_kill") == 0) {
        module_state.hook_kill_enabled = 0;
        snprintf(kernel_out, sizeof(kernel_out), "Kill hook disabled");
    }
    // Command: reset_counters - Reset hook counters
    else if (strcmp(ctl_args, "reset_counters") == 0) {
        module_state.access_hook_count = 0;
        module_state.openat_hook_count = 0;
        module_state.kill_hook_count = 0;
        snprintf(kernel_out, sizeof(kernel_out), "Counters reset");
    }
    // Command: set_whitelist - Set filter mode to whitelist
    else if (strcmp(ctl_args, "set_whitelist") == 0) {
        module_state.filter_mode = 0;
        snprintf(kernel_out, sizeof(kernel_out), "Filter mode: whitelist");
    }
    // Command: set_blacklist - Set filter mode to blacklist
    else if (strcmp(ctl_args, "set_blacklist") == 0) {
        module_state.filter_mode = 1;
        snprintf(kernel_out, sizeof(kernel_out), "Filter mode: blacklist");
    }
    // Command: clear_filters - Clear all filters
    else if (strcmp(ctl_args, "clear_filters") == 0) {
        module_state.filter_count = 0;
        for (i = 0; i < MAX_FILTERS; i++) {
            module_state.filters[i].active = 0;
            module_state.filters[i].name[0] = '\0';
            module_state.filters[i].pid = 0;
        }
        snprintf(kernel_out, sizeof(kernel_out), "All filters cleared");
    }
    // Command: add_filter:name:xxx or add_filter:pid:123
    else if (strncmp(ctl_args, "add_filter:", 11) == 0) {
        const char *filter_spec = ctl_args + 11;
        
        if (module_state.filter_count >= MAX_FILTERS) {
            snprintf(kernel_out, sizeof(kernel_out), "Error: Maximum filters reached");
            ret = -ENOMEM;
        } else if (strncmp(filter_spec, "name:", 5) == 0) {
            const char *name = filter_spec + 5;
            strncpy(module_state.filters[module_state.filter_count].name, name, MAX_NAME_LEN - 1);
            module_state.filters[module_state.filter_count].name[MAX_NAME_LEN - 1] = '\0';
            module_state.filters[module_state.filter_count].pid = 0;
            module_state.filters[module_state.filter_count].active = 1;
            snprintf(kernel_out, sizeof(kernel_out), "Added filter: name=%s", name);
            module_state.filter_count++;
        } else if (strncmp(filter_spec, "pid:", 4) == 0) {
            int pid = 0;
            const char *pid_str = filter_spec + 4;
            // Simple atoi
            while (*pid_str >= '0' && *pid_str <= '9') {
                pid = pid * 10 + (*pid_str - '0');
                pid_str++;
            }
            if (pid > 0) {
                module_state.filters[module_state.filter_count].name[0] = '\0';
                module_state.filters[module_state.filter_count].pid = pid;
                module_state.filters[module_state.filter_count].active = 1;
                snprintf(kernel_out, sizeof(kernel_out), "Added filter: pid=%d", pid);
                module_state.filter_count++;
            } else {
                snprintf(kernel_out, sizeof(kernel_out), "Error: Invalid PID");
                ret = -EINVAL;
            }
        } else {
            snprintf(kernel_out, sizeof(kernel_out), "Error: Invalid filter format");
            ret = -EINVAL;
        }
    }
    // Command: bp_set:addr:type:size:desc - Set hardware breakpoint
    // Format: bp_set:0x12345678:0:2:my_function
    // Format with PID: bp_set:0x12345678:0:2:1234:my_function
    // type: 0=exec, 1=write, 2=read, 3=rw
    // size: 0=1byte, 1=2bytes, 2=4bytes, 3=8bytes
    else if (strncmp(ctl_args, "bp_set:", 7) == 0) {
        const char *bp_spec = ctl_args + 7;
        unsigned long addr = 0;
        int type = 0, size = 2, pid = 0;  // Default: exec, 4 bytes, system-wide
        char desc[128] = {0};
        const char *p = bp_spec;
        int field = 0;
        
        // Parse address (hex)
        if (strncmp(p, "0x", 2) == 0 || strncmp(p, "0X", 2) == 0) {
            p += 2;
        }
        while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
            addr = addr * 16;
            if (*p >= '0' && *p <= '9') addr += *p - '0';
            else if (*p >= 'a' && *p <= 'f') addr += *p - 'a' + 10;
            else if (*p >= 'A' && *p <= 'F') addr += *p - 'A' + 10;
            p++;
        }
        
        // Parse remaining fields: type:size:pid:desc or type:size:desc
        while (*p == ':') {
            p++;
            field++;
            
            if (field == 1) {
                // Type
                type = *p - '0';
                p++;
            } else if (field == 2) {
                // Size
                size = *p - '0';
                p++;
            } else if (field == 3) {
                // Could be PID or description
                // If it's a number, it's PID, otherwise it's description
                if (*p >= '0' && *p <= '9') {
                    // Parse PID
                    while (*p >= '0' && *p <= '9') {
                        pid = pid * 10 + (*p - '0');
                        p++;
                    }
                } else {
                    // It's description
                    strncpy(desc, p, sizeof(desc) - 1);
                    desc[sizeof(desc) - 1] = '\0';
                    break;
                }
            } else if (field == 4) {
                // Description after PID
                strncpy(desc, p, sizeof(desc) - 1);
                desc[sizeof(desc) - 1] = '\0';
                break;
            }
        }
        
        if (addr == 0) {
            snprintf(kernel_out, sizeof(kernel_out), "Error: Invalid address");
            ret = -EINVAL;
        } else {
            int bp_idx;
            if (pid > 0) {
                bp_idx = hw_breakpoint_set_for_pid(addr, type, size, pid, desc);
            } else {
                bp_idx = hw_breakpoint_set(addr, type, size, desc);
            }
            
            if (bp_idx >= 0) {
                if (pid > 0) {
                    snprintf(kernel_out, sizeof(kernel_out), 
                             "Breakpoint[%d] set at 0x%lx for PID=%d", bp_idx, addr, pid);
                } else {
                    snprintf(kernel_out, sizeof(kernel_out), 
                             "Breakpoint[%d] set at 0x%lx (system-wide)", bp_idx, addr);
                }
            } else {
                snprintf(kernel_out, sizeof(kernel_out), 
                         "Failed to set breakpoint: %d", bp_idx);
                ret = bp_idx;
            }
        }
    }
    // Command: bp_clear:index - Clear hardware breakpoint by index
    else if (strncmp(ctl_args, "bp_clear:", 9) == 0) {
        const char *idx_str = ctl_args + 9;
        int idx = 0;
        
        // Parse index
        while (*idx_str >= '0' && *idx_str <= '9') {
            idx = idx * 10 + (*idx_str - '0');
            idx_str++;
        }
        
        int result = hw_breakpoint_clear(idx);
        if (result == 0) {
            snprintf(kernel_out, sizeof(kernel_out), "Breakpoint[%d] cleared", idx);
        } else {
            snprintf(kernel_out, sizeof(kernel_out), "Failed to clear breakpoint[%d]: %d", idx, result);
            ret = result;
        }
    }
    // Command: bp_clear_all - Clear all hardware breakpoints
    else if (strcmp(ctl_args, "bp_clear_all") == 0) {
        hw_breakpoint_clear_all();
        snprintf(kernel_out, sizeof(kernel_out), "All breakpoints cleared");
    }
    // Command: bp_verbose_on - Enable verbose breakpoint logging
    else if (strcmp(ctl_args, "bp_verbose_on") == 0) {
        hw_breakpoint_set_verbose(1);
        snprintf(kernel_out, sizeof(kernel_out), "Breakpoint verbose mode enabled");
    }
    // Command: bp_verbose_off - Disable verbose breakpoint logging
    else if (strcmp(ctl_args, "bp_verbose_off") == 0) {
        hw_breakpoint_set_verbose(0);
        snprintf(kernel_out, sizeof(kernel_out), "Breakpoint verbose mode disabled");
    }
    // Command: bp_list - List all hardware breakpoints
    else if (strcmp(ctl_args, "bp_list") == 0) {
        int len = snprintf(kernel_out, sizeof(kernel_out), "Hardware Breakpoints:\n");
        int count = 0;
        
        for (i = 0; i < MAX_HW_BREAKPOINTS && len < sizeof(kernel_out) - 100; i++) {
            struct hw_breakpoint *bp = hw_breakpoint_get(i);
            if (bp && bp->enabled) {
                const char *type_str = "unknown";
                switch (bp->type) {
                    case HW_BP_TYPE_EXEC: type_str = "exec"; break;
                    case HW_BP_TYPE_WRITE: type_str = "write"; break;
                    case HW_BP_TYPE_READ: type_str = "read"; break;
                    case HW_BP_TYPE_RW: type_str = "rw"; break;
                }
                
                len += snprintf(kernel_out + len, sizeof(kernel_out) - len,
                              "[%d] 0x%lx (%s, %d bytes, hits:%d) %s\n",
                              i, bp->addr, type_str, 1 << bp->size, bp->hit_count,
                              bp->description[0] ? bp->description : "");
                count++;
            }
        }
        
        if (count == 0) {
            len += snprintf(kernel_out + len, sizeof(kernel_out) - len, "  (none)\n");
        }
        len += snprintf(kernel_out + len, sizeof(kernel_out) - len, 
                       "Total: %d/%d slots used", count, MAX_HW_BREAKPOINTS);
    }
    // Command: mem_read:pid:addr:size - Read memory from process
    // Format: mem_read:1234:0x7f12345678:64
    else if (strncmp(ctl_args, "mem_read:", 9) == 0) {
        const char *mem_spec = ctl_args + 9;
        int pid = 0;
        unsigned long addr = 0;
        int size = 16;  // Default: 16 bytes
        const char *p = mem_spec;
        
        // Parse PID
        while (*p >= '0' && *p <= '9') {
            pid = pid * 10 + (*p - '0');
            p++;
        }
        
        // Parse address
        if (*p == ':') {
            p++;
            if (strncmp(p, "0x", 2) == 0 || strncmp(p, "0X", 2) == 0) {
                p += 2;
            }
            while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
                addr = addr * 16;
                if (*p >= '0' && *p <= '9') addr += *p - '0';
                else if (*p >= 'a' && *p <= 'f') addr += *p - 'a' + 10;
                else if (*p >= 'A' && *p <= 'F') addr += *p - 'A' + 10;
                p++;
            }
        }
        
        // Parse size
        if (*p == ':') {
            p++;
            size = 0;
            while (*p >= '0' && *p <= '9') {
                size = size * 10 + (*p - '0');
                p++;
            }
        }
        
        if (pid <= 0 || addr == 0) {
            snprintf(kernel_out, sizeof(kernel_out), "Error: Invalid PID or address");
            ret = -EINVAL;
        } else if (size <= 0 || size > 256) {
            snprintf(kernel_out, sizeof(kernel_out), "Error: Invalid size (1-256)");
            ret = -EINVAL;
        } else {
            char hex_buf[1024];
            int bytes_read = process_memory_read_hex(pid, addr, hex_buf, sizeof(hex_buf), size);
            
            if (bytes_read > 0) {
                snprintf(kernel_out, sizeof(kernel_out),
                         "Read %d bytes from PID=%d at 0x%lx:\n%s",
                         bytes_read, pid, addr, hex_buf);
            } else {
                snprintf(kernel_out, sizeof(kernel_out),
                         "Failed to read memory from PID=%d at 0x%lx: %d",
                         pid, addr, bytes_read);
                ret = bytes_read;
            }
        }
    }
    // Command: help - Show available commands
    else if (strcmp(ctl_args, "help") == 0) {
        snprintf(kernel_out, sizeof(kernel_out),
                 "Available commands:\n"
                 "  get_status        - Get module status\n"
                 "  enable            - Enable all hooks\n"
                 "  disable           - Disable all hooks\n"
                 "  enable_access     - Enable access hook\n"
                 "  disable_access    - Disable access hook\n"
                 "  enable_openat     - Enable openat hook\n"
                 "  disable_openat    - Disable openat hook\n"
                 "  enable_kill       - Enable kill hook\n"
                 "  disable_kill      - Disable kill hook\n"
                 "  reset_counters    - Reset hook counters\n"
                 "  set_whitelist     - Set filter mode to whitelist\n"
                 "  set_blacklist     - Set filter mode to blacklist\n"
                 "  add_filter:name:X - Add name filter\n"
                 "  add_filter:pid:X  - Add PID filter\n"
                 "  clear_filters     - Clear all filters\n"
                 "  bp_set:addr:type:size:pid:desc - Set hardware breakpoint\n"
                 "  bp_clear:index    - Clear breakpoint by index\n"
                 "  bp_clear_all      - Clear all breakpoints\n"
                 "  bp_list           - List all breakpoints\n"
                 "  bp_verbose_on     - Enable detailed breakpoint logging\n"
                 "  bp_verbose_off    - Disable detailed breakpoint logging\n"
                 "  mem_read:pid:addr:size - Read process memory\n"
                 "  help              - Show this help");
    }
    // Unknown command
    else {
        snprintf(kernel_out, sizeof(kernel_out), 
                 "Unknown command: %s (try 'help')", ctl_args);
        ret = -EINVAL;
    }
    
    // Copy result to userspace
    if (out_msg && outlen > 0 && g_arch_copy_to_user) {
        long copy_len = strlen(kernel_out) + 1;
        if (copy_len > outlen) {
            copy_len = outlen;
        }
        if (g_arch_copy_to_user(out_msg, kernel_out, copy_len) != 0) {
            return -EFAULT;
        }
    }
    
    return ret;
}

static long kpm_init(const char *args, const char *event, void *__user reserved)
{
    pr_info("kpm-inline-access (v10.3 with memory access) init...\n");

    // Initialize sub-modules
    if (stack_unwind_init() != 0) {
        pr_err("Failed to initialize stack unwinding\n");
        return -1;
    }

    if (process_info_init() != 0) {
        pr_err("Failed to initialize process info\n");
        return -1;
    }
    
    if (hw_breakpoint_init() != 0) {
        pr_err("Failed to initialize hardware breakpoints\n");
        return -1;
    }
    
    if (process_memory_init() != 0) {
        pr_err("Failed to initialize process memory access\n");
        return -1;
    }

    // Resolve symbols for this module
    g_strncpy_from_user = (strncpy_from_user_t)kallsyms_lookup_name("strncpy_from_user");
    g_print_vma_addr = (print_vma_addr_t)kallsyms_lookup_name("print_vma_addr");
    g_arch_copy_to_user = (arch_copy_to_user_t)kallsyms_lookup_name("__arch_copy_to_user");

    if (!g_arch_copy_to_user) {
        pr_err("Failed to resolve __arch_copy_to_user\n");
        return -1;
    }

    // Hook do_faccessat
    g_do_faccessat_addr = (void *)kallsyms_lookup_name("do_faccessat");
    if (!g_do_faccessat_addr) {
        pr_err("do_faccessat missing\n");
        return -1;
    }

    hook_err_t err = hook_wrap3(g_do_faccessat_addr, before_do_faccessat, NULL, 0);
    if (err) {
        pr_err("do_faccessat hook installation failed\n");
        return err;
    }
    pr_info("do_faccessat hook installed\n");

    // Hook do_sys_openat2 (used by openat/openat2 syscalls)
    g_do_sys_openat2_addr = (void *)kallsyms_lookup_name("do_sys_openat2");
    if (g_do_sys_openat2_addr) {
        err = hook_wrap4(g_do_sys_openat2_addr, before_do_sys_openat2, NULL, 0);
        if (err) {
            pr_warn("do_sys_openat2 hook installation failed, trying do_sys_open\n");
            g_do_sys_openat2_addr = NULL;
        } else {
            pr_info("do_sys_openat2 hook installed\n");
        }
    }

    // Fallback: try do_sys_open for older kernels
    if (!g_do_sys_openat2_addr) {
        g_do_sys_openat2_addr = (void *)kallsyms_lookup_name("do_sys_open");
        if (g_do_sys_openat2_addr) {
            err = hook_wrap4(g_do_sys_openat2_addr, before_do_sys_openat2, NULL, 0);
            if (err) {
                pr_warn("do_sys_open hook installation failed\n");
                g_do_sys_openat2_addr = NULL;
            } else {
                pr_info("do_sys_open hook installed\n");
            }
        }
    }

    // Hook sys_kill - ARM64 uses __arm64_sys_kill
    g_sys_kill_addr = (void *)kallsyms_lookup_name("__arm64_sys_kill");
    if (!g_sys_kill_addr) {
        // Fallback to generic names
        g_sys_kill_addr = (void *)kallsyms_lookup_name("__sys_kill");
    }
    if (!g_sys_kill_addr) {
        g_sys_kill_addr = (void *)kallsyms_lookup_name("sys_kill");
    }
    
    if (g_sys_kill_addr) {
        // ARM64 syscalls use hook_wrap1 because they take pt_regs as single argument
        err = hook_wrap1(g_sys_kill_addr, before_sys_kill, NULL, 0);
        if (err) {
            pr_warn("sys_kill hook installation failed\n");
            g_sys_kill_addr = NULL;
        } else {
            pr_info("sys_kill hook installed at %p\n", g_sys_kill_addr);
        }
    } else {
        pr_warn("sys_kill symbol not found, kill hook disabled\n");
    }

    pr_info("Hook initialization complete. Supercall control enabled.\n");
    pr_info("Use 'kpm control kpm-inline-access help' to see available commands\n");
    pr_info("NOTE: All hooks are DISABLED by default. Use 'enable' command to activate.\n");
    return 0;
}

static long kpm_exit(void *__user reserved)
{
    // Clear all hardware breakpoints
    hw_breakpoint_clear_all();
    
    if (g_do_faccessat_addr) {
        unhook(g_do_faccessat_addr);
        pr_info("do_faccessat hook removed\n");
    }
    
    if (g_do_sys_openat2_addr) {
        unhook(g_do_sys_openat2_addr);
        pr_info("openat hook removed\n");
    }
    
    if (g_sys_kill_addr) {
        unhook(g_sys_kill_addr);
        pr_info("sys_kill hook removed\n");
    }
    
    pr_info("Final statistics: access=%d, openat=%d, kill=%d\n",
            module_state.access_hook_count,
            module_state.openat_hook_count,
            module_state.kill_hook_count);
    
    return 0;
}

KPM_INIT(kpm_init);
KPM_CTL0(kpm_control);
KPM_EXIT(kpm_exit);