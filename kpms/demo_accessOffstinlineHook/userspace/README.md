# 用户态控制程序

用于控制 `kpm-inline-access` 模块的用户态程序。

## 快速开始

### 编译

```cmd
build.bat
```

### 安装

```bash
adb push libs/arm64-v8a/kpm_control /data/local/tmp/
adb shell chmod 755 /data/local/tmp/kpm_control
```

### 基本使用

```bash
# 查看状态
./kpm_control <superkey> get_status

# 启用/禁用所有 hooks
./kpm_control <superkey> enable
./kpm_control <superkey> disable

# 查看帮助
./kpm_control <superkey> help
```

将 `<superkey>` 替换为你的实际超级密钥（例如：`pengxintu123`）

## 过滤功能

### 设置过滤模式

```bash
# 白名单模式（只 hook 匹配的进程）
./kpm_control <superkey> set_whitelist

# 黑名单模式（排除匹配的进程）
./kpm_control <superkey> set_blacklist
```

### 添加过滤器

```bash
# 按包名/进程名过滤
./kpm_control <superkey> add_name com.example.app

# 按 PID 过滤
./kpm_control <superkey> add_pid 1234

# 清除所有过滤器
./kpm_control <superkey> clear_filters
```

### 控制特定系统调用

```bash
# 控制 access() hook
./kpm_control <superkey> enable_access
./kpm_control <superkey> disable_access

# 控制 openat() hook
./kpm_control <superkey> enable_openat
./kpm_control <superkey> disable_openat
```

## 完整文档

详细的使用指南请参考：
- `../FILTER_GUIDE.md` - 过滤功能详细说明
- `../RELOAD_MODULE.md` - 模块重新加载指南
- `../SUCCESS_SUMMARY.md` - 技术总结和问题解决

## 工具说明

### kpm_control
主控制程序，用于与内核模块通信。

### version_test
测试 KernelPatch 版本和认证。

### syscall_test
测试并找到正确的 syscall 号。

## 配置

### 超级密钥
- 标准 KernelPatch: 使用你设置的超级密钥
- APatch: 可能需要使用 `su` 作为密钥

### Syscall 号
当前配置为 45（标准 KernelPatch）。如果需要修改，编辑 `jni/scdefs.h`。

### 版本
当前配置为 0.10.7。如果需要修改，编辑 `jni/version`。

## 故障排除

### 认证失败
```bash
# 运行 syscall 测试
./syscall_test <superkey>

# 或尝试使用 su
./kpm_control su get_status
```

### 模块未加载
```bash
# 检查模块
adb shell su -c 'kpm list'

# 加载模块
adb shell su -c 'kpm load /data/local/tmp/accessOffstinlineHook.kpm'
```

### 命令不生效
模块可能是旧版本，需要重新加载。参考 `../RELOAD_MODULE.md`。
