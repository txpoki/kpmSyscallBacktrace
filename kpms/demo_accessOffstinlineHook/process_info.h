/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Process information functions
 */

#ifndef _PROCESS_INFO_H_
#define _PROCESS_INFO_H_

#include "common.h"

// Initialize process info module
int process_info_init(void);

// Get process ID
pid_t get_process_id(struct task_struct *task);

// Get process command line or name
void get_process_cmdline(struct task_struct *task, char *buf, size_t buf_len);

#endif /* _PROCESS_INFO_H_ */
