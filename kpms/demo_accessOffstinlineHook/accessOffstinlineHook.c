/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <hook.h>
#include "common.h"
#include "stack_unwind.h"
#include "process_info.h"



KPM_NAME("kpm-inline-access");
KPM_VERSION("9.0.0");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("bmax121 & User");
KPM_DESCRIPTION("Inline Hook with print_vma_addr");

// Global variables for this module
static strncpy_from_user_t g_strncpy_from_user = NULL;
static print_vma_addr_t g_print_vma_addr = NULL;
static void *g_do_faccessat_addr = NULL;

void before_do_faccessat(hook_fargs3_t *args, void *udata)
{
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

    struct task_struct *task = current;
    get_process_cmdline(task, pkg_name, sizeof(pkg_name));

    pr_info("INLINE_ACCESS: [%s] (PID:%d) -> %s [Mode:%d]\n", 
            pkg_name, get_process_id(task), path_buf, mode);

    unwind_user_stack_standard(task);
}

static long kpm_init(const char *args, const char *event, void *__user reserved)
{
    pr_info("kpm-inline-access (v9.0 print_vma_addr) init...\n");

    // Initialize sub-modules
    if (stack_unwind_init() != 0) {
        pr_err("Failed to initialize stack unwinding\n");
        return -1;
    }

    if (process_info_init() != 0) {
        pr_err("Failed to initialize process info\n");
        return -1;
    }

    // Resolve symbols for this module
    g_strncpy_from_user = (strncpy_from_user_t)kallsyms_lookup_name("strncpy_from_user");
    g_print_vma_addr = (print_vma_addr_t)kallsyms_lookup_name("print_vma_addr");

    g_do_faccessat_addr = (void *)kallsyms_lookup_name("do_faccessat");
    if (!g_do_faccessat_addr) {
        pr_err("do_faccessat missing\n");
        return -1;
    }

    // Install hook
    hook_err_t err = hook_wrap3(g_do_faccessat_addr, before_do_faccessat, NULL, 0);
    if (err) {
        pr_err("Hook installation failed\n");
        return err;
    }

    pr_info("Hook success.\n");
    return 0;
}

static long kpm_exit(void *__user reserved)
{
    if (g_do_faccessat_addr) {
        unhook(g_do_faccessat_addr);
    }
    return 0;
}

KPM_INIT(kpm_init);
KPM_EXIT(kpm_exit);
