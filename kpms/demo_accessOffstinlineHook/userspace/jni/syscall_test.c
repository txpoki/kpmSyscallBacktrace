/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Syscall Number Test - 测试不同的 syscall 号
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <errno.h>

#define SUPERCALL_HELLO 0x1000
#define SUPERCALL_HELLO_MAGIC 0x11581158

/**
 * Hash function for old KernelPatch versions
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
    return (hash & 0xFFFF0000) | cmd;
}

static inline long test_syscall_number(const char *key, int syscall_nr)
{
    // Try hash-based encoding
    long ret = syscall(syscall_nr, key, hash_key_cmd(key, SUPERCALL_HELLO));
    return ret;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <superkey>\n", argv[0]);
        printf("This tool tests different syscall numbers to find the correct one.\n");
        return 1;
    }

    const char *key = argv[1];
    
    printf("Testing different syscall numbers...\n");
    printf("Superkey: %s\n", key);
    printf("==============================================\n\n");

    // Common syscall numbers to test
    int syscall_numbers[] = {
        45,      // Standard KernelPatch (__NR3264_truncate)
        0x1ee,   // 494 - Alternative
        46,      // __NR3264_ftruncate
        44,      // __NR_renameat
        0x1ed,   // 493
        0x1ef,   // 495
        0x1ec,   // 492
        0x1f0,   // 496
    };
    
    int num_tests = sizeof(syscall_numbers) / sizeof(syscall_numbers[0]);
    int found = 0;
    
    for (int i = 0; i < num_tests; i++) {
        int nr = syscall_numbers[i];
        errno = 0;
        long ret = test_syscall_number(key, nr);
        
        printf("Syscall %3d (0x%03x): ret=0x%016lx", nr, nr, ret);
        
        if (ret == SUPERCALL_HELLO_MAGIC) {
            printf(" - SUCCESS! ✓\n");
            printf("\n==============================================\n");
            printf("Found working syscall number: %d (0x%x)\n", nr, nr);
            printf("\nUpdate scdefs.h with:\n");
            printf("  #define __NR_supercall %d\n", nr);
            found = 1;
            break;
        } else {
            printf(" - failed (errno=%d: %s)\n", errno, strerror(errno));
        }
    }
    
    if (!found) {
        printf("\n==============================================\n");
        printf("No working syscall number found.\n\n");
        
        printf("Possible reasons:\n");
        printf("1. Wrong superkey\n");
        printf("2. KernelPatch/APatch not installed\n");
        printf("3. Need root permission\n");
        printf("4. Syscall number is different on your device\n\n");
        
        printf("Try:\n");
        printf("  su -c '%s su'\n", argv[0]);
        printf("  su -c '%s %s'\n", argv[0], key);
        printf("\nCheck kernel logs:\n");
        printf("  dmesg | grep -i \"supercall\\|kernelpatch\\|apatch\"\n");
    }

    return found ? 0 : 1;
}
