/* SPDX-License-Identifier: GPL-2.0-or-later */
/* 
 * Supercall header for KernelPatch/APatch
 * Based on user_deprecated/supercall.h with compact_cmd support
 */

#ifndef _KPU_SUPERCALL_H_
#define _KPU_SUPERCALL_H_

#include <unistd.h>
#include <sys/syscall.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "scdefs.h"
#include "version"

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

/**
 * @deprecated
 * KernelPatch version less than 0xa05
 */
static inline long hash_key_cmd(const char *key, long cmd)
{
    long hash = hash_key(key);
    return hash & 0xFFFF0000 | cmd;
}

/**
 * KernelPatch version is greater than or equal to 0x0a05
 */
static inline long ver_and_cmd(const char *key, long cmd)
{
    uint32_t version_code = (MAJOR << 16) + (MINOR << 8) + PATCH;
    return ((long)version_code << 32) | (0x1158 << 16) | (cmd & 0xFFFF);
}

/**
 * Compact command - automatically detects KernelPatch version
 * and uses the appropriate command encoding
 */
static inline long compact_cmd(const char *key, long cmd)
{
#if 1
    // Try to get KernelPatch version
    long ver = syscall(__NR_supercall, key, ver_and_cmd(key, SUPERCALL_KERNELPATCH_VER));
    if (ver >= 0xa05) {
        // Use new version encoding
        return ver_and_cmd(key, cmd);
    }
#endif
    // Fall back to old hash-based encoding
    return hash_key_cmd(key, cmd);
}

/**
 * @brief If KernelPatch installed, @see SUPERCALL_HELLO_MAGIC will echoed.
 * 
 * @param key : superkey or 'su' string if caller uid is su allowed 
 * @return long 
 */
static inline long sc_hello(const char *key)
{
    if (!key || !key[0]) return -EINVAL;
    long ret = syscall(__NR_supercall, key, compact_cmd(key, SUPERCALL_HELLO));
    return ret;
}

/**
 * @brief Is KernelPatch installed?
 * 
 * @param key : superkey or 'su' string if caller uid is su allowed 
 * @return true 
 * @return false 
 */
static inline bool sc_ready(const char *key)
{
    return sc_hello(key) == SUPERCALL_HELLO_MAGIC;
}

/**
 * @brief KernelPatch version number
 * 
 * @param key 
 * @return uint32_t 
 */
static inline uint32_t sc_kp_ver(const char *key)
{
    if (!key || !key[0]) return -EINVAL;
    long ret = syscall(__NR_supercall, key, compact_cmd(key, SUPERCALL_KERNELPATCH_VER));
    return (uint32_t)ret;
}

/**
 * @brief Control module with arguments 
 * 
 * @param key : superkey
 * @param name : module name
 * @param ctl_args : control argument
 * @param out_msg : output message buffer
 * @param outlen : buffer length of out_msg
 * @return long : 0 if succeed
 */
static inline long sc_kpm_control(const char *key, const char *name, const char *ctl_args, char *out_msg, long outlen)
{
    if (!key || !key[0]) return -EINVAL;
    if (!name || strlen(name) <= 0) return -EINVAL;
    if (!ctl_args || strlen(ctl_args) <= 0) return -EINVAL;
    long ret = syscall(__NR_supercall, key, compact_cmd(key, SUPERCALL_KPM_CONTROL), name, ctl_args, out_msg, outlen);
    return ret;
}

#endif
