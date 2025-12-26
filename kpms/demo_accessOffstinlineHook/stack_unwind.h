/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Stack unwinding functions
 */

#ifndef _STACK_UNWIND_H_
#define _STACK_UNWIND_H_

#include "common.h"

// Initialize stack unwinding module
int stack_unwind_init(void);

// Perform user-space stack unwinding
void unwind_user_stack_standard(struct task_struct *task);

// Get VMA information string for an address
int get_vma_info_str(unsigned long ip, char *buf, size_t len);

// Helper function
const char *my_kbasename(const char *path);

#endif /* _STACK_UNWIND_H_ */
