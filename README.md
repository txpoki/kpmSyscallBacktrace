# KPM 系统调用回溯追踪模块

[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

基于 KernelPatch 框架开发的系统调用监控和回溯追踪模块，用于 Android 系统的安全分析和逆向工程。

## 项目概述

这是一个 KPM (KernelPatch Module) 模块，通过内联 Hook 技术监控系统调用，并提供用户空间调用栈回溯功能。能够追踪应用程序的文件访问行为，并解析调用栈中每个地址对应的库文件和偏移信息。

## 核心功能

- **系统调用 Hook**: 监控 `do_faccessat` 文件访问系统调用
- **栈回溯**: 支持 ARM64 和 ARM32 (Thumb/ARM) 模式的用户栈回溯
- **VMA 解析**: 解析虚拟内存区域，显示库文件名和偏移
- **进程信息**: 记录进程名、PID、文件路径和访问模式

## 输出示例

```
[MyHook] INLINE_ACCESS: [com.example.app] (PID:1234) -> /data/data/com.example.app/files/config.txt [Mode:0]
[MyHook] #00 PC: 00007b2c4a8f20 libc.so + 0x2f20
[MyHook] #01 PC: 00007b2c4a8f40 libc.so + 0x2f40  
[MyHook] #02 PC: 00007b2c5a1234 libapp.so + 0x1234
[MyHook] ------------------------------------------
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
dmesg | grep MyHook  # 查看日志
```

## 项目结构

```
├── kpms/demo_accessOffstinlineHook/
│   ├── accessOffstinlineHook.c    # 主模块源码
│   ├── Makefile                   # 构建配置
│   └── accessOffstinlineHook.kpm  # 编译后的模块
├── kernel/                        # KernelPatch 内核代码
├── tools/                         # 构建工具
└── user/                         # 用户空间接口
```

## TODO List

### 🎯 硬件断点支持
- [ ] 集成 ARM64 硬件断点功能
- [ ] 支持数据访问断点 (Watchpoint)
- [ ] 支持指令执行断点 (Breakpoint)
- [ ] 实现断点条件过滤机制
- [ ] 添加断点管理接口

### 🔗 用户态通信
- [ ] 实现内核模块与用户态程序通信机制
- [ ] 添加 procfs 接口用于配置和状态查询
- [ ] 支持实时日志输出到用户态
- [ ] 实现用户态控制命令接口
- [ ] 添加配置文件热加载功能

### 📊 系统调用扩展
- [ ] 支持更多系统调用的 Hook 追踪
  - [ ] `openat` - 文件打开监控
  - [ ] `read`/`write` - 文件读写监控
  - [ ] `mmap`/`munmap` - 内存映射监控
  - [ ] `execve` - 进程执行监控
  - [ ] `connect`/`bind` - 网络连接监控
- [ ] 可配置的系统调用过滤器
- [ ] 支持系统调用参数详细解析
- [ ] 添加系统调用统计功能

### 🛠️ 功能增强
- [ ] 支持多进程/线程过滤
- [ ] 添加调用栈深度配置
- [ ] 实现日志级别控制
- [ ] 支持符号表缓存优化
- [ ] 添加性能统计和监控

### 🔧 稳定性改进
- [ ] 优化内核版本兼容性
- [ ] 添加更多错误处理机制
- [ ] 实现模块热重载功能
- [ ] 添加内存泄漏检测
- [ ] 完善异常恢复机制

## 应用场景

- **安全分析**: 监控恶意软件文件访问行为
- **逆向工程**: 追踪 API 调用和代码执行流程
- **性能分析**: 识别热点文件访问路径

## 许可证

本项目采用 GPL v2 许可证。详见 [LICENSE](LICENSE) 文件。

---

**免责声明**: 本工具仅供安全研究和学习使用。