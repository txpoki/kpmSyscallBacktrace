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
    printf("Examples:\n");
    printf("  %s su get_status\n", prog);
    printf("  %s su disable\n", prog);
    printf("  %s su set_whitelist\n", prog);
    printf("  %s su add_name com.example.app\n", prog);
    printf("  %s su add_pid 1234\n", prog);
    printf("  %s su clear_filters\n", prog);
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
