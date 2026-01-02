/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * KPM Control - 用户态控制程序
 * 用于控制 kpm-inline-access 模块
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "supercall.h"

#define MODULE_NAME "kpm-inline-access"
#define OUT_BUF_SIZE 2048

static void print_usage(const char *prog)
{
    printf("Usage: %s <superkey> <command> [args]\n", prog);
    printf("\n");
    printf("Basic Commands:\n");
    printf("  get_status        - Get module status and filters\n");
    printf("  enable            - Enable all hooks\n");
    printf("  disable           - Disable all hooks\n");
    printf("  reset_counters    - Reset hook counters\n");
    printf("  help              - Show module help\n");
    printf("\n");
    printf("Hook Control:\n");
    printf("  enable_access     - Enable access() hook\n");
    printf("  disable_access    - Disable access() hook\n");
    printf("  enable_openat     - Enable openat() hook\n");
    printf("  disable_openat    - Disable openat() hook\n");
    printf("  enable_kill       - Enable kill() hook\n");
    printf("  disable_kill      - Disable kill() hook\n");
    printf("\n");
    printf("Filter Commands:\n");
    printf("  set_whitelist     - Set filter mode to whitelist (only hook filtered)\n");
    printf("  set_blacklist     - Set filter mode to blacklist (skip filtered)\n");
    printf("  add_name <name>   - Add package/process name filter\n");
    printf("  add_pid <pid>     - Add PID filter\n");
    printf("  clear_filters     - Clear all filters\n");
    printf("\n");
    printf("Hardware Breakpoint Commands:\n");
    printf("  bp_set <addr> <type> <size> [pid] [desc] - Set hardware breakpoint\n");
    printf("    addr: Address in hex (e.g., 0x12345678)\n");
    printf("    type: 0=exec, 1=write, 2=read, 3=rw\n");
    printf("    size: 0=1byte, 1=2bytes, 2=4bytes, 3=8bytes\n");
    printf("    pid:  Optional PID (0 or omit for system-wide)\n");
    printf("    desc: Optional description\n");
    printf("  bp_clear <index>  - Clear breakpoint by index (0-3)\n");
    printf("  bp_clear_all      - Clear all breakpoints\n");
    printf("  bp_list           - List all breakpoints\n");
    printf("  bp_verbose_on     - Enable detailed logging (WARNING: may cause issues)\n");
    printf("  bp_verbose_off    - Disable detailed logging (default, safe)\n");
    printf("\n");
    printf("Memory Access Commands:\n");
    printf("  mem_read <pid> <addr> <size> - Read memory from process\n");
    printf("    pid:  Target process PID\n");
    printf("    addr: Memory address in hex (e.g., 0x7f12345678)\n");
    printf("    size: Number of bytes to read (1-256)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s su get_status\n", prog);
    printf("  %s su disable\n", prog);
    printf("  %s su set_whitelist\n", prog);
    printf("  %s su add_name com.example.app\n", prog);
    printf("  %s su add_pid 1234\n", prog);
    printf("  %s su bp_set 0x7f12345678 0 2 my_function\n", prog);
    printf("  %s su bp_set 0x7f12345678 0 2 1234 my_function_in_pid_1234\n", prog);
    printf("  %s su bp_list\n", prog);
    printf("  %s su bp_clear 0\n", prog);
    printf("  %s su mem_read 1234 0x7f12345678 64\n", prog);
    printf("\n");
    printf("Filter Modes:\n");
    printf("  whitelist - Only hook processes matching filters\n");
    printf("  blacklist - Hook all processes except those matching filters\n");
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *key = argv[1];
    const char *command = argv[2];
    char out_msg[OUT_BUF_SIZE] = {0};
    char full_command[512] = {0};
    long ret;

    // 检查 KernelPatch 是否安装
    if (!sc_ready(key)) {
        fprintf(stderr, "Error: KernelPatch not installed or invalid superkey\n");
        return 1;
    }

    printf("KernelPatch detected, version: 0x%08x\n", sc_kp_ver(key));
    
    // Handle special commands that need arguments
    if (strcmp(command, "add_name") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: add_name requires a name argument\n");
            fprintf(stderr, "Usage: %s <key> add_name <package_name>\n", argv[0]);
            return 1;
        }
        snprintf(full_command, sizeof(full_command), "add_filter:name:%s", argv[3]);
        command = full_command;
    } else if (strcmp(command, "add_pid") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: add_pid requires a PID argument\n");
            fprintf(stderr, "Usage: %s <key> add_pid <pid>\n", argv[0]);
            return 1;
        }
        snprintf(full_command, sizeof(full_command), "add_filter:pid:%s", argv[3]);
        command = full_command;
    } else if (strcmp(command, "bp_set") == 0) {
        // bp_set <addr> <type> <size> [pid] [desc]
        if (argc < 6) {
            fprintf(stderr, "Error: bp_set requires address, type, and size\n");
            fprintf(stderr, "Usage: %s <key> bp_set <addr> <type> <size> [pid] [desc]\n", argv[0]);
            fprintf(stderr, "  addr: hex address (e.g., 0x12345678)\n");
            fprintf(stderr, "  type: 0=exec, 1=write, 2=read, 3=rw\n");
            fprintf(stderr, "  size: 0=1byte, 1=2bytes, 2=4bytes, 3=8bytes\n");
            fprintf(stderr, "  pid:  optional PID (0 for system-wide)\n");
            fprintf(stderr, "  desc: optional description\n");
            return 1;
        }
        
        // Check if we have PID and/or description
        if (argc >= 8) {
            // addr type size pid desc
            snprintf(full_command, sizeof(full_command), "bp_set:%s:%s:%s:%s:%s", 
                    argv[3], argv[4], argv[5], argv[6], argv[7]);
        } else if (argc >= 7) {
            // Could be: addr type size pid OR addr type size desc
            // Check if argv[6] is a number (PID) or text (description)
            int is_number = 1;
            const char *test = argv[6];
            while (*test) {
                if (*test < '0' || *test > '9') {
                    is_number = 0;
                    break;
                }
                test++;
            }
            
            if (is_number) {
                // It's a PID without description
                snprintf(full_command, sizeof(full_command), "bp_set:%s:%s:%s:%s:", 
                        argv[3], argv[4], argv[5], argv[6]);
            } else {
                // It's a description without PID
                snprintf(full_command, sizeof(full_command), "bp_set:%s:%s:%s:%s", 
                        argv[3], argv[4], argv[5], argv[6]);
            }
        } else {
            // addr type size only
            snprintf(full_command, sizeof(full_command), "bp_set:%s:%s:%s:", 
                    argv[3], argv[4], argv[5]);
        }
        command = full_command;
    } else if (strcmp(command, "bp_clear") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: bp_clear requires an index\n");
            fprintf(stderr, "Usage: %s <key> bp_clear <index>\n", argv[0]);
            return 1;
        }
        snprintf(full_command, sizeof(full_command), "bp_clear:%s", argv[3]);
        command = full_command;
    } else if (strcmp(command, "mem_read") == 0) {
        // mem_read <pid> <addr> <size>
        if (argc < 6) {
            fprintf(stderr, "Error: mem_read requires PID, address, and size\n");
            fprintf(stderr, "Usage: %s <key> mem_read <pid> <addr> <size>\n", argv[0]);
            fprintf(stderr, "  pid:  target process PID\n");
            fprintf(stderr, "  addr: hex address (e.g., 0x7f12345678)\n");
            fprintf(stderr, "  size: number of bytes (1-256)\n");
            return 1;
        }
        snprintf(full_command, sizeof(full_command), "mem_read:%s:%s:%s", 
                argv[3], argv[4], argv[5]);
        command = full_command;
    }
    
    printf("Sending command to module '%s': %s\n", MODULE_NAME, command);
    printf("========================================\n");

    // 调用模块的控制接口
    ret = sc_kpm_control(key, MODULE_NAME, command, out_msg, sizeof(out_msg));

    if (ret < 0) {
        fprintf(stderr, "Error: sc_kpm_control failed with code %ld (%s)\n", 
                ret, strerror(-ret));
        
        if (ret == -ENOENT) {
            fprintf(stderr, "Module '%s' not loaded. Load it first with:\n", MODULE_NAME);
            fprintf(stderr, "  kpm load /path/to/accessOffstinlineHook.kpm\n");
        }
        
        return 1;
    }

    // 打印返回的消息
    printf("%s\n", out_msg);
    printf("========================================\n");
    printf("Command executed successfully\n");

    return 0;
}
