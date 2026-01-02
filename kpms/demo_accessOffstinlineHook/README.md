# KPM Inline Hook with Filtering

内核模块，用于 hook `access()` 和 `openat()` 系统调用，支持灵活的过滤和控制功能。

## 功能特性

- ✅ Hook `do_faccessat` (access 系统调用)
- ✅ Hook `do_sys_openat2` (openat 系统调用)
- ✅ 用户栈回溯（ARM32）
- ✅ 进程信息获取
- ✅ Supercall 控制接口
- ✅ 包名/进程名过滤
- ✅ PID 过滤
- ✅ 白名单/黑名单模式
- ✅ 独立的系统调用控制

## 快速开始

### 1. 编译内核模块

```bash
cd kpms/demo_accessOffstinlineHook
make
```

### 2. 编译用户态工具

```cmd
cd userspace
build.bat
```

### 3. 部署

```bash
# 推送模块
adb push accessOffstinlineHook.kpm /data/local/tmp/
adb shell su -c 'kpm load /data/local/tmp/accessOffstinlineHook.kpm'

# 推送控制工具
adb push userspace/libs/arm64-v8a/kpm_control /data/local/tmp/
adb shell chmod 755 /data/local/tmp/kpm_control
```

### 4. 使用

```bash
# 查看状态
adb shell /data/local/tmp/kpm_control pengxintu123 get_status

# 设置白名单模式，只监控特定应用
adb shell /data/local/tmp/kpm_control pengxintu123 set_whitelist
adb shell /data/local/tmp/kpm_control pengxintu123 add_name com.example.app

# 查看日志
adb shell dmesg | grep "INLINE_"
```

## 文档

- **[FILTER_GUIDE.md](FILTER_GUIDE.md)** - 过滤功能详细使用指南
- **[RELOAD_MODULE.md](RELOAD_MODULE.md)** - 模块重新加载指南
- **[SUCCESS_SUMMARY.md](SUCCESS_SUMMARY.md)** - 技术总结和问题解决历程
- **[userspace/README.md](userspace/README.md)** - 用户态工具说明

## 使用场景

### 场景 1: 监控特定应用的文件访问

```bash
# 设置白名单模式
./kpm_control pengxintu123 set_whitelist

# 添加要监控的应用
./kpm_control pengxintu123 add_name com.tencent.mm

# 只监控 openat（文件打开）
./kpm_control pengxintu123 disable_access

# 查看日志
dmesg | grep "INLINE_OPENAT.*tencent"
```

### 场景 2: 排除系统进程

```bash
# 设置黑名单模式
./kpm_control pengxintu123 set_blacklist

# 排除系统进程
./kpm_control pengxintu123 add_name system_server
./kpm_control pengxintu123 add_name surfaceflinger
```

### 场景 3: 监控特定 PID

```bash
# 获取 PID
PID=$(adb shell pidof com.example.app)

# 添加 PID 过滤
./kpm_control pengxintu123 set_whitelist
./kpm_control pengxintu123 add_pid $PID
```

## 命令参考

| 命令 | 说明 |
|------|------|
| `get_status` | 查看模块状态和过滤器 |
| `enable` / `disable` | 启用/禁用所有 hooks |
| `enable_access` / `disable_access` | 控制 access hook |
| `enable_openat` / `disable_openat` | 控制 openat hook |
| `set_whitelist` / `set_blacklist` | 设置过滤模式 |
| `add_name <name>` | 添加名称过滤器 |
| `add_pid <pid>` | 添加 PID 过滤器 |
| `clear_filters` | 清除所有过滤器 |
| `reset_counters` | 重置计数器 |
| `help` | 显示帮助 |

## 架构

```
accessOffstinlineHook.kpm (内核模块)
├── accessOffstinlineHook.c  - 主模块和控制接口
├── stack_unwind.c/h         - 栈回溯实现
├── process_info.c/h         - 进程信息获取
└── common.h                 - 共享定义

userspace/ (用户态工具)
├── jni/
│   ├── kpm_control.c        - 控制程序
│   ├── version_test.c       - 版本测试
│   ├── syscall_test.c       - Syscall 测试
│   ├── supercall.h          - Supercall 接口
│   └── scdefs.h             - Supercall 定义
└── libs/arm64-v8a/
    └── kpm_control          - 编译后的二进制
```

## 技术细节

### Hook 实现
- 使用 KernelPatch 的 `hook_wrap3` 和 `hook_wrap4`
- Inline hook，不修改函数代码
- 在函数入口处拦截

### 栈回溯
- ARM32 架构
- 基于帧指针（FP）
- 支持 ARM 和 Thumb 混合模式
- 解析 VMA 信息显示库名和偏移

### 过滤机制
- 最多 16 个过滤器
- 支持部分匹配（strstr）
- 白名单：只 hook 匹配的
- 黑名单：排除匹配的

### Supercall 接口
- Syscall 号: 45
- 自动版本检测（compact_cmd）
- 支持旧版本（hash-based）和新版本（version-based）

## 性能考虑

- 使用白名单模式监控少数应用性能影响最小
- 禁用不需要的 hook 减少开销
- 过滤器使用简单的字符串匹配，性能良好
- 栈回溯有一定开销，建议只在需要时启用

## 兼容性

- **内核**: Linux 4.x - 5.x
- **架构**: ARM32
- **KernelPatch**: 0.10.x - 0.12.x
- **APatch**: 支持

## 开发

### 修改代码后重新加载

```bash
# 编译
make

# 重新加载
./reload.bat  # Windows
# 或
./reload.sh   # Linux/Mac
```

详见 [RELOAD_MODULE.md](RELOAD_MODULE.md)

## 许可证

GPL v2

## 作者

bmax121 & User

## 版本

v10.0.0 - 添加过滤功能和独立的系统调用控制
