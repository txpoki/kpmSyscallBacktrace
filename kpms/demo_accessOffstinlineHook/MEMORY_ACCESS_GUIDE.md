# Process Memory Access Guide

## Overview

The module now supports reading memory from any process's address space. This is useful for:
- Inspecting variables and data structures
- Debugging and reverse engineering
- Monitoring memory changes
- Extracting configuration data

## Command

```bash
./kpm_control <superkey> mem_read <pid> <address> <size>
```

**Parameters:**
- `pid`: Target process ID
- `address`: Memory address in hexadecimal (e.g., `0x7f12345678`)
- `size`: Number of bytes to read (1-256)

## Examples

### 1. Read 16 bytes from an address

```bash
# Get the PID of target app
PID=$(adb shell pidof com.example.app)

# Read 16 bytes from address 0x7f12345678
./kpm_control pengxintu123 mem_read $PID 0x7f12345678 16
```

**Output:**
```
Read 16 bytes from PID=1234 at 0x7f12345678:
48 65 6c 6c 6f 20 57 6f 72 6c 64 21 00 00 00 00
```

### 2. Read a larger block of memory

```bash
# Read 64 bytes
./kpm_control pengxintu123 mem_read 1234 0x7f00001000 64
```

### 3. Read from different memory regions

```bash
# Read from code section
./kpm_control pengxintu123 mem_read 1234 0x7f12345678 32

# Read from data section
./kpm_control pengxintu123 mem_read 1234 0x7f00001000 16

# Read from heap
./kpm_control pengxintu123 mem_read 1234 0x7e00000000 64

# Read from stack
./kpm_control pengxintu123 mem_read 1234 0x7ffffff000 32
```

## Finding Memory Addresses

### Method 1: Using /proc/[pid]/maps

```bash
# View memory mappings
adb shell cat /proc/1234/maps

# Example output:
# 7f00000000-7f00001000 rw-p 00000000 00:00 0    [heap]
# 7f12340000-7f12350000 r-xp 00000000 b3:1a 123  /system/lib64/libexample.so
# 7f12350000-7f12351000 rw-p 00010000 b3:1a 123  /system/lib64/libexample.so
```

### Method 2: Using hardware breakpoints

```bash
# Set a breakpoint to find when code executes
./kpm_control pengxintu123 bp_set 0x7f12345678 0 2 1234 "find_address"

# Check dmesg for breakpoint hits
dmesg | grep "HW_BREAKPOINT"
```

### Method 3: Using IDA Pro or similar tools

1. Open the APK/library in IDA Pro
2. Find the function or variable you want to inspect
3. Note the offset from the base address
4. Add the offset to the base address from `/proc/[pid]/maps`

## Use Cases

### 1. Inspect Global Variables

```bash
# Find global variable address from /proc/[pid]/maps
# Assume libexample.so is loaded at 0x7f12340000
# And global_config is at offset 0x5000

ADDR=$((0x7f12340000 + 0x5000))
./kpm_control pengxintu123 mem_read 1234 $(printf "0x%x" $ADDR) 32
```

### 2. Monitor String Data

```bash
# Read a string (null-terminated)
./kpm_control pengxintu123 mem_read 1234 0x7f00001000 64

# Convert hex to ASCII (in your terminal)
# 48 65 6c 6c 6f = "Hello"
```

### 3. Extract Configuration

```bash
# Read configuration structure
./kpm_control pengxintu123 mem_read 1234 0x7f00002000 128

# Parse the hex output to understand the structure
```

### 4. Verify Memory Modifications

```bash
# Before modification
./kpm_control pengxintu123 mem_read 1234 0x7f00001000 16

# ... make changes ...

# After modification
./kpm_control pengxintu123 mem_read 1234 0x7f00001000 16
```

### 5. Combine with Hardware Breakpoints

```bash
# Set breakpoint on function
./kpm_control pengxintu123 bp_set 0x7f12345678 0 2 1234 "my_function"

# When breakpoint hits, read nearby memory
./kpm_control pengxintu123 mem_read 1234 0x7f12345678 64

# Read function arguments (from stack)
./kpm_control pengxintu123 mem_read 1234 0x7ffffff000 32
```

## Limitations

1. **Maximum read size**: 256 bytes per request
2. **Valid addresses only**: Reading from invalid addresses will fail
3. **Process must exist**: Target PID must be valid
4. **Permissions**: Requires kernel-level access (via KPM)
5. **No write support**: Currently read-only (for safety)

## Error Handling

### "Process not found"
```
Error: Process with PID 1234 not found
```
- The process doesn't exist or has terminated
- Check PID with `ps | grep <process_name>`

### "Failed to read memory"
```
Error: Failed to read memory from PID=1234 at 0x7f12345678
```
- Invalid address (not mapped in process)
- Check `/proc/[pid]/maps` for valid ranges
- Address might be protected or unmapped

### "Read 0 bytes"
```
Warning: Read 0 bytes from PID=1234 at 0x7f12345678
```
- Address is not accessible
- Page might not be present in memory
- Try a different address

## Tips

1. **Start small**: Read 16-32 bytes first to verify the address is valid
2. **Check maps**: Always verify addresses in `/proc/[pid]/maps`
3. **Use hex calculator**: Convert offsets carefully
4. **Combine tools**: Use with breakpoints for dynamic analysis
5. **Log everything**: Keep track of addresses and their contents

## Security Considerations

- This feature requires kernel-level access
- Only use on processes you own or have permission to inspect
- Be careful with sensitive data (passwords, keys, etc.)
- Memory contents may contain private information

## Advanced: Scripting

```bash
#!/bin/bash
# Script to dump multiple memory regions

PID=$1
SUPERKEY="pengxintu123"

# Read code section
echo "=== Code Section ==="
./kpm_control $SUPERKEY mem_read $PID 0x7f12340000 64

# Read data section
echo "=== Data Section ==="
./kpm_control $SUPERKEY mem_read $PID 0x7f12350000 64

# Read heap
echo "=== Heap ==="
./kpm_control $SUPERKEY mem_read $PID 0x7e00000000 64
```

## Troubleshooting

**Q: Why do I get "Process memory functions not available"?**
A: The kernel doesn't export `access_process_vm`. This is rare but possible on some custom kernels.

**Q: Can I read from kernel space?**
A: No, this function only reads from user space addresses of the target process.

**Q: How do I convert hex output to readable text?**
A: Use online hex-to-ASCII converters or write a simple script:
```bash
echo "48 65 6c 6c 6f" | xxd -r -p
# Output: Hello
```

**Q: Can I write to process memory?**
A: Not with this implementation (read-only for safety). Writing would require additional functions and careful error handling.
