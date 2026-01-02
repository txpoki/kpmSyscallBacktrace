/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Process Memory Access
 */

#ifndef _PROCESS_MEMORY_H_
#define _PROCESS_MEMORY_H_

#include "common.h"

// Maximum size for a single read operation
#define MAX_READ_SIZE 4096

// Initialize process memory subsystem
int process_memory_init(void);

// Read memory from a process
// Returns: number of bytes read, or negative error code
int process_memory_read(int pid, unsigned long addr, void *buf, size_t size);

// Read and format memory as hex string
// Returns: number of bytes read, or negative error code
int process_memory_read_hex(int pid, unsigned long addr, char *out, size_t out_size, size_t read_size);

#endif /* _PROCESS_MEMORY_H_ */
