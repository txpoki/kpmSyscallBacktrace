# Hardware Breakpoint Guide

## Overview

The module now supports ARM64 hardware breakpoints, allowing you to monitor code execution and data access at specific memory addresses.

## Features

- Up to 4 hardware breakpoints (device dependent)
- Execution breakpoints (break on code execution)
- Data breakpoints (break on read/write access)
- Automatic stack unwinding on breakpoint hit
- Process information logging

## Commands

### Set a Breakpoint

```bash
./kpm_control <superkey> bp_set <address> <type> <size> [description]
```

**Parameters:**
- `address`: Memory address in hexadecimal (e.g., `0x7f12345678`)
- `type`: Breakpoint type
  - `0` = Execution (break when code at this address executes)
  - `1` = Write (break when data is written)
  - `2` = Read (break when data is read)
  - `3` = Read/Write (break on any access)
- `size`: Data size (for data breakpoints)
  - `0` = 1 byte
  - `1` = 2 bytes
  - `2` = 4 bytes
  - `3` = 8 bytes
- `description`: Optional text description

**Examples:**
```bash
# Break on execution at address 0x7f12345678
./kpm_control su bp_set 0x7f12345678 0 2 "my_function"

# Break on 4-byte write to address 0x7f00001000
./kpm_control su bp_set 0x7f00001000 1 2 "global_var"

# Break on 8-byte read/write to address 0x7f00002000
./kpm_control su bp_set 0x7f00002000 3 3 "shared_data"
```

### List Breakpoints

```bash
./kpm_control <superkey> bp_list
```

Shows all active breakpoints with their addresses, types, hit counts, and descriptions.

**Example output:**
```
Hardware Breakpoints:
[0] 0x7f12345678 (exec, 4 bytes, hits:5) my_function
[1] 0x7f00001000 (write, 4 bytes, hits:0) global_var
Total: 2/4 slots used
```

### Clear a Breakpoint

```bash
./kpm_control <superkey> bp_clear <index>
```

Clears the breakpoint at the specified index (0-3).

**Example:**
```bash
./kpm_control su bp_clear 0
```

### Clear All Breakpoints

```bash
./kpm_control <superkey> bp_clear_all
```

Removes all hardware breakpoints.

### Verbose Mode Control

**Note:** Verbose mode is not currently implemented due to complexity in KernelPatch environment.

The commands are available but will only show a warning:

```bash
# These commands exist but verbose mode is not functional
./kpm_control <superkey> bp_verbose_on   # Shows warning
./kpm_control <superkey> bp_verbose_off  # Confirms minimal mode
```

All breakpoints use **minimal logging mode** which is the safest approach.

## Breakpoint Hit Information

When a breakpoint is hit, the kernel log shows minimal information for safety:

```
HW_BP[0]: Hit at 0x7f12345678, count:1
HW_BP[0]: Hit at 0x7f12345678, count:2
HW_BP[0]: Hit at 0x7f12345678, count:3
```

This minimal logging is **safe** and won't cause system freezes. It includes:
- Breakpoint index [0-3]
- Address where breakpoint triggered
- Hit count

**Why minimal logging?**

Hardware breakpoint handlers run in interrupt context where complex operations (getting PID, process name, stack traces) can cause:
- System freezes
- Deadlocks
- Kernel crashes

The minimal approach ensures reliability and safety.

## Use Cases

### 1. Monitor Function Execution

Set an execution breakpoint at a function's entry point to track when it's called:

```bash
# Get function address from /proc/<pid>/maps or objdump
./kpm_control su bp_set 0x7f12345678 0 2 "onCreate"
```

### 2. Track Data Modifications

Monitor when a global variable or data structure is modified:

```bash
# Break on writes to a specific address
./kpm_control su bp_set 0x7f00001000 1 2 "config_flag"
```

### 3. Debug Memory Corruption

Set read/write breakpoints on memory regions to catch corruption:

```bash
# Monitor all access to a buffer
./kpm_control su bp_set 0x7f00002000 3 3 "buffer_start"
```

### 4. Reverse Engineering

Track execution flow by setting breakpoints at key functions:

```bash
./kpm_control su bp_set 0x7f12340000 0 2 "decrypt_function"
./kpm_control su bp_set 0x7f12350000 0 2 "verify_license"
./kpm_control su bp_list
```

## Limitations

1. **Hardware Limit**: Most ARM64 devices support 4-6 hardware breakpoints total
2. **System-wide**: Breakpoints affect all processes (use filters to limit scope)
3. **Performance**: Frequent breakpoint hits can impact system performance
4. **Address Space**: Breakpoints work on virtual addresses (process-specific)

## Tips

1. **Use with Filters**: Combine with process filters to limit breakpoint scope:
   ```bash
   ./kpm_control su set_whitelist
   ./kpm_control su add_name com.example.app
   ./kpm_control su bp_set 0x7f12345678 0 2 "target_function"
   ```

2. **Check Availability**: Use `bp_list` to see how many slots are available

3. **Clear When Done**: Always clear breakpoints when finished to free resources:
   ```bash
   ./kpm_control su bp_clear_all
   ```

4. **Monitor Logs**: Use `dmesg` or `logcat` to see breakpoint hit information:
   ```bash
   dmesg | grep "HW_BREAKPOINT"
   ```

## Troubleshooting

**"Failed to set breakpoint"**: 
- All breakpoint slots are in use (clear some first)
- Invalid address or parameters
- Hardware doesn't support the requested breakpoint type

**No breakpoint hits**:
- Address might be wrong (check with `/proc/<pid>/maps`)
- Process might not be executing that code path
- Breakpoint might be in wrong address space (different process)

**System freezes when breakpoint triggers**:
- This should not happen with the current minimal logging implementation
- If it does, check kernel logs for other errors
- Try clearing all breakpoints: `bp_clear_all`

**RCU warnings in kernel log**:
- These warnings are normal and can be ignored
- They occur because breakpoints trigger in interrupt context

**System instability**:
- Too many breakpoint hits causing performance issues
- Clear breakpoints and use more specific addresses or filters
