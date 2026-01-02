/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Hardware Breakpoint Support for ARM64
 */

#ifndef _HW_BREAKPOINT_H_
#define _HW_BREAKPOINT_H_

#include "common.h"

// ARM64 supports up to 16 hardware breakpoints (implementation defined)
// Most devices have 4-6 breakpoints
#define MAX_HW_BREAKPOINTS 4

// Breakpoint types
#define HW_BP_TYPE_EXEC   0  // Execution breakpoint
#define HW_BP_TYPE_WRITE  1  // Data write breakpoint
#define HW_BP_TYPE_READ   2  // Data read breakpoint
#define HW_BP_TYPE_RW     3  // Data read/write breakpoint

// Breakpoint sizes
#define HW_BP_SIZE_1      0  // 1 byte
#define HW_BP_SIZE_2      1  // 2 bytes
#define HW_BP_SIZE_4      2  // 4 bytes
#define HW_BP_SIZE_8      3  // 8 bytes

struct hw_breakpoint {
    unsigned long addr;      // Breakpoint address
    int type;                // Breakpoint type
    int size;                // Breakpoint size
    int enabled;             // Is this breakpoint active?
    int hit_count;           // Number of times hit
    char description[128];   // Description
};

// Initialize hardware breakpoint subsystem
int hw_breakpoint_init(void);

// Set a hardware breakpoint
// Returns: breakpoint index (0-3) on success, negative on error
int hw_breakpoint_set(unsigned long addr, int type, int size, const char *desc);

// Set a hardware breakpoint for a specific process (by PID)
// Returns: breakpoint index (0-3) on success, negative on error
int hw_breakpoint_set_for_pid(unsigned long addr, int type, int size, int pid, const char *desc);

// Clear a hardware breakpoint by index
int hw_breakpoint_clear(int index);

// Clear all hardware breakpoints
void hw_breakpoint_clear_all(void);

// Get breakpoint info
struct hw_breakpoint *hw_breakpoint_get(int index);

// Get number of available breakpoints
int hw_breakpoint_get_count(void);

// Set verbose mode (0=minimal logging, 1=detailed logging with stack traces)
void hw_breakpoint_set_verbose(int verbose);

// Get verbose mode
int hw_breakpoint_get_verbose(void);

#endif /* _HW_BREAKPOINT_H_ */
