# 🎉 Supercall 控制功能成功实现

## 问题解决历程

### 问题 1: 版本认证失败 ❌ → ✅
**症状**: `0xffffffffffffffff` 返回值，所有版本测试失败

**原因**: 
- 使用了错误的 syscall 号 (0x1ee/494)
- 应该使用 45

**解决方案**:
1. 创建 `syscall_test` 工具自动检测正确的 syscall 号
2. 更新 `scdefs.h` 中的 `__NR_supercall` 为 45
3. 实现 `compact_cmd` 自动版本检测（支持新旧两种编码方式）

### 问题 2: 模块控制失败 ❌ → ✅
**症状**: `sc_kpm_control failed with code -1 (Operation not permitted)`

**原因**: 
- 错误的函数签名：第一个参数使用了 `const char *__user args`
- 实际上 KernelPatch 已经将参数复制到内核空间
- 应该使用 `const char *ctl_args`（内核空间指针）

**解决方案**:
通过分析 `kernel/patch/module/module.c` 和 `kpms/demo_supercall` 的实现，发现：

**错误的实现**:
```c
static long kpm_control(const char *__user args, char *__user out_msg, int outlen)
{
    char kernel_args[256];
    // 错误：尝试从用户空间复制，但 args 已经在内核空间
    if (copy_from_user(kernel_args, args, sizeof(kernel_args) - 1) != 0) {
        return -EFAULT;
    }
    // ...
}
```

**正确的实现**:
```c
static long kpm_control(const char *ctl_args, char *__user out_msg, int outlen)
{
    char kernel_out[1024];
    // 正确：ctl_args 已经在内核空间，直接使用
    pr_info("[Control] Received command: %s\n", ctl_args);
    
    if (strcmp(ctl_args, "get_status") == 0) {
        snprintf(kernel_out, sizeof(kernel_out), "...");
    }
    
    // 只需要 copy_to_user 将结果返回给用户空间
    if (g_arch_copy_to_user(out_msg, kernel_out, copy_len) != 0) {
        return -EFAULT;
    }
    return 0;
}
```

**关键发现**:
- KernelPatch 的 `module_control0` 函数接收用户空间的参数
- 然后复制到 `mod->ctl_args`（内核空间）
- 最后调用模块的 `ctl0` 函数时，传递的是内核空间指针
- 因此模块不需要（也不应该）再次使用 `copy_from_user`

## 最终配置

### 内核模块 (accessOffstinlineHook.c)

```c
// 正确的函数签名
static long kpm_control(const char *ctl_args, char *__user out_msg, int outlen)

// 注册控制接口
KPM_CTL0(kpm_control);
```

### 用户态程序 (kpm_control.c)

```c
// 使用 compact_cmd 自动适配版本
ret = sc_kpm_control(key, MODULE_NAME, command, out_msg, sizeof(out_msg));
```

### Supercall 配置 (scdefs.h)

```c
// 正确的 syscall 号
#define __NR_supercall 45
```

## 测试结果

### ✅ 所有功能正常工作

```bash
# 获取状态
$ ./kpm_control pengxintu123 get_status
KernelPatch detected, version: 0x00000c02
Sending command to module 'kpm-inline-access': get_status
========================================
enabled=1
access_count=0
openat_count=0
total_hooks=0
========================================
Command executed successfully

# 禁用 hooks
$ ./kpm_control pengxintu123 disable
========================================
Hooks disabled
========================================
Command executed successfully

# 启用 hooks
$ ./kpm_control pengxintu123 enable
========================================
Hooks enabled
========================================
Command executed successfully

# 重置计数器
$ ./kpm_control pengxintu123 reset_counters
========================================
Counters reset
========================================
Command executed successfully

# 获取帮助
$ ./kpm_control pengxintu123 help
========================================
Available commands:
  get_status        - Get module status
  enable            - Enable hooks
  disable           - Disable hooks
  reset_counters    - Reset hook counters
  get_access_count  - Get access hook count
  get_openat_count  - Get openat hook count
  help              - Show this help
========================================
Command executed successfully
```

## 技术要点总结

### 1. Syscall 号检测
- 使用 `syscall_test` 工具自动检测
- 标准 KernelPatch 使用 45 (truncate)
- 某些设备可能使用其他号码

### 2. 版本自动适配
- `compact_cmd` 函数自动检测 KernelPatch 版本
- 支持旧版本 (< 0xa05) 的 hash-based 编码
- 支持新版本 (>= 0xa05) 的 version-based 编码

### 3. 内核模块控制接口
- 第一个参数是**内核空间指针**，不需要 `__user` 修饰
- 不需要 `copy_from_user`，直接使用
- 第二个参数是**用户空间指针**，需要 `copy_to_user`
- 返回值：0 表示成功，负数表示错误码

### 4. 函数签名对比

| 组件 | 函数签名 | 说明 |
|------|----------|------|
| 内核定义 | `long (*mod_ctl0call_t)(const char *ctl_args, char *__user out_msg, int outlen)` | kpmodule.h |
| 内核调用 | `module_control0(name, ctl_args, out_msg, outlen)` | 用户空间参数 |
| 内核处理 | `strcpy(mod->ctl_args, ctl_args)` | 复制到内核空间 |
| 模块接收 | `kpm_control(mod->ctl_args, out_msg, outlen)` | 内核空间参数 |

## 文件清单

### 内核模块
- `kpms/demo_accessOffstinlineHook/accessOffstinlineHook.c` - 主模块（已修复）
- `kpms/demo_accessOffstinlineHook/stack_unwind.c` - 栈回溯
- `kpms/demo_accessOffstinlineHook/process_info.c` - 进程信息
- `kpms/demo_accessOffstinlineHook/common.h` - 共享定义
- `kpms/demo_accessOffstinlineHook/Makefile` - 编译配置

### 用户态工具
- `userspace/jni/kpm_control.c` - 控制程序
- `userspace/jni/version_test.c` - 版本测试工具
- `userspace/jni/syscall_test.c` - Syscall 号测试工具
- `userspace/jni/supercall.h` - Supercall 接口（使用 compact_cmd）
- `userspace/jni/scdefs.h` - Supercall 定义（syscall 号 = 45）
- `userspace/jni/version` - 版本配置
- `userspace/build.bat` - Windows 编译脚本

### 文档
- `SUCCESS_SUMMARY.md` - 本文档
- `FINAL_GUIDE.md` - 使用指南
- `SYSCALL_FIX.md` - Syscall 号问题修复
- `MODULE_NOT_LOADED.md` - 模块加载问题
- `UPDATED_SUPERCALL.md` - Supercall 更新说明

## 使用方法

### 编译内核模块
```bash
cd kpms/demo_accessOffstinlineHook
make
```

### 编译用户态工具
```cmd
cd userspace
build.bat
```

### 部署和使用
```bash
# 推送模块
adb push accessOffstinlineHook.kpm /data/local/tmp/
adb shell su -c 'kpm load /data/local/tmp/accessOffstinlineHook.kpm'

# 推送控制工具
adb push userspace/libs/arm64-v8a/kpm_control /data/local/tmp/
adb shell chmod 755 /data/local/tmp/kpm_control

# 使用控制工具
adb shell /data/local/tmp/kpm_control pengxintu123 get_status
adb shell /data/local/tmp/kpm_control pengxintu123 disable
adb shell /data/local/tmp/kpm_control pengxintu123 enable
```

## 学到的经验

1. **仔细阅读内核源码** - 通过分析 `kernel/patch/module/module.c` 找到了问题根源
2. **参考官方示例** - `kpms/demo_supercall` 提供了正确的实现参考
3. **理解内核/用户空间边界** - 明确哪些指针在内核空间，哪些在用户空间
4. **使用诊断工具** - `syscall_test` 和 `version_test` 帮助快速定位问题
5. **版本兼容性** - `compact_cmd` 确保兼容不同版本的 KernelPatch

## 致谢

感谢 bmax121 的 KernelPatch 项目提供了强大的内核模块框架和 supercall 机制。

---

**状态**: ✅ 完全成功
**日期**: 2025-12-29
**版本**: v9.0.0 with supercall
