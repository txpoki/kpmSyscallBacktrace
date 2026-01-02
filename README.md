# KPM 系统调用回溯追踪模块

[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

基于 KernelPatch 框架开发的系统调用监控、硬件断点和内存访问模块，用于 Android 系统的安全分析和逆向工程。

## 项目概述

这是一个功能强大的 KPM (KernelPatch Module) 模块，提供系统调用监控、硬件断点、进程内存读取等高级调试功能。通过内联 Hook 技术监控系统调用，并提供用户空间调用栈回溯功能。能够追踪应用程序的文件访问行为，设置硬件断点进行精确调试，并解析调用栈中每个地址对应的库文件和偏移信息。

## 核心功能

### 系统调用 Hook
- **系统调用监控**: 监控 `access`、`openat`、`kill` 等系统调用
- **灵活过滤**: 支持包名/进程名过滤、PID 过滤、白名单/黑名单模式
- **独立控制**: 每个系统调用可独立启用/禁用
- **栈回溯**: 支持 ARM64 和 ARM32 (Thumb/ARM) 模式的用户栈回溯
- **VMA 解析**: 解析虚拟内存区域，显示库文件名和偏移
- **进程信息**: 记录进程名、PID、文件路径和访问模式

### 硬件断点（ARM64）✨
- **执行断点**: 在指令执行时触发
- **数据断点**: 监控内存读取、写入或读写操作
- **系统级/进程级**: 支持全局断点和针对特定进程的断点
- **Move-to-Next-Instruction 机制**: 断点可多次触发，指令正常执行
- **自动回退**: 如果 `modify_user_hw_breakpoint` 不可用，自动使用一次性断点模式
- **最多 4 个硬件断点**: ARM64 硬件限制

### 进程内存访问
- **内存读取**: 读取任意进程的内存内容（最多 256 字节）
- **十六进制输出**: 以十六进制字符串格式返回内存数据
- **安全访问**: 使用 `access_process_vm` 安全访问进程内存

## 输出示例

### 系统调用监控

```
[MyHook] INLINE_ACCESS: [com.example.app] (PID:1234) -> /data/data/com.example.app/files/config.txt [Mode:0]
[MyHook] #00 PC: 00007b2c4a8f20 libc.so + 0x2f20
[MyHook] #01 PC: 00007b2c4a8f40 libc.so + 0x2f40  
[MyHook] #02 PC: 00007b2c5a1234 libapp.so + 0x1234
[MyHook] ------------------------------------------

[MyHook] INLINE_OPENAT: [com.android.systemui] (PID:5678) -> /system/framework/services.jar [DFD:-100]
[MyHook] #00 PC: 00007b2c4b1000 libc.so + 0x3000
[MyHook] #01 PC: 00007b2c5c2000 libandroid_runtime.so + 0x2000
[MyHook] ------------------------------------------
```

### 硬件断点

```bash
# 设置执行断点
$ kpm_control pengxintu123 bp_set 0x755d685794 0 4 0 "test_breakpoint"
Hardware breakpoint[0] set at 0x755d685794 (system-wide, type=0, size=4)

# 断点触发日志
HW_BP[0]: Hit at 0x755d685794 (original), count:1
HW_BP[0]: Moved to next instruction 0x755d685798
HW_BP[0]: Hit at 0x755d685798 (next instruction)
HW_BP[0]: Moved back to original 0x755d685794

# 列出断点
$ kpm_control pengxintu123 bp_list
[0] 0x755d685794 (exec, 4 bytes, hits:1) test_breakpoint
Total: 1/4 slots used
```

### 进程内存读取

```bash
$ kpm_control pengxintu123 mem_read 1234 0x7b2c4a8f20 16
Memory read result: 7f454c4602010100000000000000000000000000
```

## 编译和使用

### 环境要求

- 交叉编译器: `aarch64-linux-gnu-gcc`
- 已安装 KernelPatch 的 Android 设备
- Root 权限

### 编译

```bash
cd kpms/demo_accessOffstinlineHook
make
make push  # 推送到设备
```

### 加载模块

```bash
# 加载模块
adb shell su -c 'kpm load /data/local/tmp/accessOffstinlineHook.kpm'

# 推送控制工具
cd userspace
build.bat
adb push libs/arm64-v8a/kpm_control /data/local/tmp/
adb shell chmod 755 /data/local/tmp/kpm_control

# 使用控制工具
adb shell /data/local/tmp/kpm_control pengxintu123 get_status
adb shell /data/local/tmp/kpm_control pengxintu123 enable

# 查看日志
adb shell dmesg | grep MyHook
```

### 快速开始

详细使用说明请参考：
- **[kpms/demo_accessOffstinlineHook/README.md](kpms/demo_accessOffstinlineHook/README.md)** - 完整使用指南
- **[kpms/demo_accessOffstinlineHook/HARDWARE_BREAKPOINT_GUIDE.md](kpms/demo_accessOffstinlineHook/HARDWARE_BREAKPOINT_GUIDE.md)** - 硬件断点指南
- **[kpms/demo_accessOffstinlineHook/FILTER_GUIDE.md](kpms/demo_accessOffstinlineHook/FILTER_GUIDE.md)** - 过滤功能指南

## 项目结构

```
├── kpms/demo_accessOffstinlineHook/
│   ├── accessOffstinlineHook.c    # 主模块源码
│   ├── hw_breakpoint.c/h          # 硬件断点实现
│   ├── process_memory.c/h         # 进程内存访问
│   ├── stack_unwind.c/h           # 栈回溯实现
│   ├── process_info.c/h           # 进程信息获取
│   ├── Makefile                   # 构建配置
│   ├── README.md                  # 完整使用指南
│   ├── HARDWARE_BREAKPOINT_GUIDE.md  # 硬件断点指南
│   ├── MOVE_TO_NEXT_INSTRUCTION.md   # 核心技术文档
│   └── userspace/                 # 用户态控制工具
│       └── jni/kpm_control.c      # 控制程序
├── kernel/                        # KernelPatch 内核代码
├── tools/                         # 构建工具
└── user/                         # 用户空间接口
```

## TODO List

### 🎯 硬件断点支持
- [x] 集成 ARM64 硬件断点功能 ✅
- [x] 支持数据访问断点 (Watchpoint) ✅
- [x] 支持指令执行断点 (Breakpoint) ✅
- [x] 实现 Move-to-Next-Instruction 机制 ✅
- [x] 添加断点管理接口 ✅
- [ ] 实现断点条件过滤机制
- [ ] 支持断点命中时的自定义回调

### 🔗 用户态通信
- [x] 实现内核模块与用户态程序通信机制 ✅ (Supercall)
- [x] 实现用户态控制命令接口 ✅ (kpm_control)
- [x] 支持配置和状态查询 ✅
- [ ] 添加 procfs 接口用于配置和状态查询
- [ ] 支持实时日志输出到用户态
- [ ] 添加配置文件热加载功能

### 📊 系统调用扩展
- [ ] 支持更多系统调用的 Hook 追踪
  - [x] `faccessat` - 文件访问检查监控 ✅
  - [x] `openat` - 文件打开监控 ✅
  - [ ] `read`/`write` - 文件读写监控
  - [ ] `mmap`/`munmap` - 内存映射监控
  - [ ] `execve` - 进程执行监控
  - [ ] `connect`/`bind` - 网络连接监控
- [ ] 可配置的系统调用过滤器
- [ ] 支持系统调用参数详细解析
- [ ] 添加系统调用统计功能

### 🛠️ 功能增强
- [x] 支持多进程/线程过滤 ✅
- [x] 实现日志级别控制 ✅
- [x] 进程内存读取功能 ✅
- [ ] 添加调用栈深度配置
- [ ] 支持符号表缓存优化
- [ ] 添加性能统计和监控
- [ ] 进程内存写入功能

### 🔧 稳定性改进
- [ ] 优化内核版本兼容性
- [ ] 添加更多错误处理机制
- [ ] 实现模块热重载功能
- [ ] 添加内存泄漏检测
- [ ] 完善异常恢复机制

## 应用场景

- **安全分析**: 监控恶意软件文件访问行为
- **逆向工程**: 追踪 API 调用和代码执行流程，使用硬件断点精确定位关键代码
- **性能分析**: 识别热点文件访问路径
- **动态调试**: 使用硬件断点进行无侵入式调试
- **内存分析**: 读取进程内存进行数据分析
- **漏洞研究**: 监控特定内存地址的访问行为

## 常见问题

### 为什么有时候符号解析失败？

这是正常现象，主要原因是锁竞争。详见 [FAQ.md](kpms/demo_accessOffstinlineHook/FAQ.md) 和 [SYMBOL_RESOLUTION_ISSUES.md](kpms/demo_accessOffstinlineHook/SYMBOL_RESOLUTION_ISSUES.md)

### 更多文档

#### 用户指南
- [kpms/demo_accessOffstinlineHook/README.md](kpms/demo_accessOffstinlineHook/README.md) - 完整使用指南
- [kpms/demo_accessOffstinlineHook/HARDWARE_BREAKPOINT_GUIDE.md](kpms/demo_accessOffstinlineHook/HARDWARE_BREAKPOINT_GUIDE.md) - 硬件断点使用指南
- [kpms/demo_accessOffstinlineHook/FILTER_GUIDE.md](kpms/demo_accessOffstinlineHook/FILTER_GUIDE.md) - 过滤功能详细指南
- [kpms/demo_accessOffstinlineHook/MEMORY_ACCESS_GUIDE.md](kpms/demo_accessOffstinlineHook/MEMORY_ACCESS_GUIDE.md) - 内存访问指南
- [kpms/demo_accessOffstinlineHook/RELOAD_MODULE.md](kpms/demo_accessOffstinlineHook/RELOAD_MODULE.md) - 模块重载指南

#### 技术文档
- [kpms/demo_accessOffstinlineHook/MOVE_TO_NEXT_INSTRUCTION.md](kpms/demo_accessOffstinlineHook/MOVE_TO_NEXT_INSTRUCTION.md) - Move-to-Next-Instruction 机制详解
- [kpms/demo_accessOffstinlineHook/INTERRUPT_CONTEXT_RULES.md](kpms/demo_accessOffstinlineHook/INTERRUPT_CONTEXT_RULES.md) - 中断上下文编程规则

## 技术亮点

### Move-to-Next-Instruction 机制

这是硬件断点实现的核心创新，解决了执行断点的无限触发问题：

1. **第一次触发**（在原地址）：使用 `modify_user_hw_breakpoint` 将断点移到下一条指令（PC+4）
2. **第二次触发**（在下一条指令）：将断点移回原地址
3. **结果**：指令正常执行，断点可以多次触发，避免无限循环

详见 [MOVE_TO_NEXT_INSTRUCTION.md](kpms/demo_accessOffstinlineHook/MOVE_TO_NEXT_INSTRUCTION.md)

## 版本历史

- **v10.3.0** (2026-01) - 添加进程内存读取功能
- **v10.2.0** (2026-01) - 添加硬件断点功能，实现 Move-to-Next-Instruction 机制
- **v10.1.0** (2025-12) - 添加过滤功能和独立的系统调用控制
- **v10.0.0** (2025-12) - 初始版本，支持 access/openat/kill hook

## 许可证

本项目采用 GPL v2 许可证。详见 [LICENSE](LICENSE) 文件。

---

**免责声明**: 本工具仅供安全研究和学习使用。