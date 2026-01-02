/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Version Test - 测试不同的 KernelPatch 版本和认证方式
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <errno.h>

#include "scdefs.h"

/**
 * Hash function for old KernelPatch versions (< 0xa05)
 */
static inline long hash_key(const char *key)
{
    long hash = 1000000007;
    for (int i = 0; key[i]; i++) {
        hash = hash * 31 + key[i];
    }
    return hash;
}

static inline long hash_key_cmd(const char *key, long cmd)
{
    long hash = hash_key(key);
    return hash & 0xFFFF0000 | cmd;
}

static inline long ver_and_cmd(int major, int minor, int patch, long cmd)
{
    uint32_t version_code = (major << 16) + (minor << 8) + patch;
    return ((long)version_code << 32) | (0x1158 << 16) | (cmd & 0xFFFF);
}

static inline long test_hash_hello(const char *key)
{
    // Try old hash-based encoding
    long ret = syscall(__NR_supercall, key, hash_key_cmd(key, SUPERCALL_HELLO));
    return ret;
}

static inline long test_version_hello(const char *key, int major, int minor, int patch)
{
    // Try new version-based encoding
    long ret = syscall(__NR_supercall, key, ver_and_cmd(major, minor, patch, SUPERCALL_HELLO));
    return ret;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <superkey>\n", argv[0]);
        printf("This tool tests different KernelPatch versions to find the correct one.\n");
        printf("\nFor APatch, try:\n");
        printf("  %s su\n", argv[0]);
        printf("  %s <your_superkey>\n", argv[0]);
        return 1;
    }

    const char *key = argv[1];
    
    printf("Testing KernelPatch/APatch authentication...\n");
    printf("Superkey: %s\n", key);
    printf("==============================================\n\n");

    // Test 1: Old hash-based encoding (KernelPatch < 0xa05)
    printf("[Test 1] Old hash-based encoding...\n");
    long ret = test_hash_hello(key);
    printf("Result: 0x%lx", ret);
    if (ret == SUPERCALL_HELLO_MAGIC) {
        printf(" - SUCCESS!\n");
        printf("\nYour KernelPatch uses OLD hash-based encoding (< 0xa05)\n");
        printf("This is compatible with the updated supercall.h\n");
        return 0;
    } else {
        printf(" - failed (errno=%d)\n\n", errno);
    }

    // Test 2: New version-based encoding
    printf("[Test 2] New version-based encoding...\n");
    int found = 0;
    
    for (int major = 0; major <= 0; major++) {
        for (int minor = 9; minor <= 13; minor++) {
            for (int patch = 0; patch <= 9; patch++) {
                long ret = test_version_hello(key, major, minor, patch);
                
                if (ret == SUPERCALL_HELLO_MAGIC) {
                    printf("✓ Version %d.%d.%d - SUCCESS!\n", major, minor, patch);
                    printf("\nFound working version!\n");
                    printf("Update jni/version file with:\n");
                    printf("  #define MAJOR %d\n", major);
                    printf("  #define MINOR %d\n", minor);
                    printf("  #define PATCH %d\n", patch);
                    found = 1;
                    break;
                } else if (patch == 0) {
                    // Only print first failure of each minor version
                    printf("✗ Version %d.%d.x - failed (ret=0x%lx, errno=%d)\n", 
                           major, minor, ret, errno);
                }
            }
            if (found) break;
        }
        if (found) break;
    }

    if (!found) {
        printf("\n==============================================\n");
        printf("No working version found.\n\n");
        
        printf("Possible reasons:\n");
        printf("1. Wrong superkey - check APatch settings\n");
        printf("2. Need root/su permission first\n");
        printf("3. APatch uses different supercall mechanism\n");
        printf("4. Syscall number might be different (current: 0x%x)\n", __NR_supercall);
        printf("\n");
        
        printf("Try these commands:\n");
        printf("  su -c '%s su'  # Try with 'su' as key\n", argv[0]);
        printf("  su -c '%s %s'  # Try with root permission\n", argv[0], key);
        printf("\n");
        
        printf("Check APatch configuration:\n");
        printf("  cat /proc/kallsyms | grep supercall\n");
        printf("  dmesg | grep -i \"apatch\\|kernelpatch\"\n");
    }

    return found ? 0 : 1;
}
