/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Process information implementation
 */

#include "process_info.h"



// Global function pointers
static get_task_comm_t g_get_task_comm = NULL;
static task_pid_nr_ns_t g_task_pid_nr_ns = NULL;
static get_cmdline_t g_get_cmdline = NULL;

pid_t get_process_id(struct task_struct *task)
{
    if (g_task_pid_nr_ns) {
        return g_task_pid_nr_ns(task, PIDTYPE_TGID, NULL);
    }
    return -1;
}

void get_process_cmdline(struct task_struct *task, char *buf, size_t buf_len)
{
    memset(buf, 0, buf_len);
    if (g_get_cmdline && g_get_cmdline(task, buf, buf_len) > 0) {
        return;
    }
    if (g_get_task_comm) {
        g_get_task_comm(buf, buf_len, task);
    } else {
        strncpy(buf, "[Unknown]", buf_len);
    }
}

int process_info_init(void)
{
    g_get_task_comm = (get_task_comm_t)kallsyms_lookup_name("__get_task_comm");
    g_task_pid_nr_ns = (task_pid_nr_ns_t)kallsyms_lookup_name("__task_pid_nr_ns");
    g_get_cmdline = (get_cmdline_t)kallsyms_lookup_name("get_cmdline");

    if (!g_task_pid_nr_ns) {
        pr_warn("__task_pid_nr_ns symbol not found\n");
    }

    return 0;
}
