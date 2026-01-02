# 模块重新加载指南

## 问题

当你修改了模块代码并重新编译后，设备上运行的仍然是旧版本的模块。

## 原因

KernelPatch 模块加载后会一直运行，直到：
1. 手动卸载
2. 设备重启

即使你推送了新的 .kpm 文件，旧模块仍在内存中运行。

## 解决方案

### 方法 1: 使用重新加载脚本（推荐）

#### Windows
```cmd
cd kpms\demo_accessOffstinlineHook
reload.bat
```

#### Linux/Mac
```bash
cd kpms/demo_accessOffstinlineHook
chmod +x reload.sh
./reload.sh
```

脚本会自动：
1. 卸载旧模块
2. 推送新模块
3. 加载新模块
4. 验证加载成功

### 方法 2: 手动操作

```bash
# 1. 卸载旧模块
adb shell su -c 'kpm unload kpm-inline-access'

# 2. 推送新模块
adb push accessOffstinlineHook.kpm /data/local/tmp/

# 3. 加载新模块
adb shell su -c 'kpm load /data/local/tmp/accessOffstinlineHook.kpm'

# 4. 验证
adb shell su -c 'kpm info kpm-inline-access'
```

### 方法 3: 重启设备

如果模块无法卸载（某些情况下可能发生），重启设备：

```bash
adb reboot
# 等待设备重启后
adb push accessOffstinlineHook.kpm /data/local/tmp/
adb shell su -c 'kpm load /data/local/tmp/accessOffstinlineHook.kpm'
```

## 验证模块版本

### 检查模块信息
```bash
adb shell su -c 'kpm info kpm-inline-access'
```

应该显示：
```
name: kpm-inline-access
version: 10.0.0
license: GPL v2
author: bmax121 & User
description: Inline Hook with filtering and supercall control
```

### 测试新功能
```bash
adb shell /data/local/tmp/kpm_control pengxintu123 help
```

应该显示新的命令列表，包括：
- `enable_access` / `disable_access`
- `enable_openat` / `disable_openat`
- `set_whitelist` / `set_blacklist`
- `add_filter:name:X`
- `add_filter:pid:X`
- `clear_filters`

### 检查内核日志
```bash
adb shell dmesg | grep "kpm-inline-access"
```

应该看到类似：
```
[12345.678] kpm-inline-access (v10.0 with filtering) init...
[12345.679] do_faccessat hook installed
[12345.680] do_sys_openat2 hook installed
[12345.681] Hook initialization complete. Supercall control enabled.
```

## 常见问题

### Q: 卸载失败 "module in use"

**A:** 模块可能正在被使用。尝试：
```bash
# 先禁用 hooks
adb shell /data/local/tmp/kpm_control pengxintu123 disable

# 等待几秒
sleep 3

# 再卸载
adb shell su -c 'kpm unload kpm-inline-access'
```

### Q: 加载失败 "module already loaded"

**A:** 旧模块还在运行。必须先卸载：
```bash
adb shell su -c 'kpm unload kpm-inline-access'
adb shell su -c 'kpm load /data/local/tmp/accessOffstinlineHook.kpm'
```

### Q: help 命令显示旧的命令列表

**A:** 这确认了旧模块还在运行。按照上面的步骤重新加载模块。

### Q: 找不到 kpm 命令

**A:** 确保 KernelPatch/APatch 已正确安装：
```bash
adb shell which kpm
adb shell su -c 'which kpm'
```

如果找不到，可能需要使用完整路径：
```bash
adb shell su -c '/data/adb/kp/bin/kpm list'
```

## 完整的开发工作流程

```bash
# 1. 修改代码
vim accessOffstinlineHook.c

# 2. 编译
make

# 3. 重新加载（使用脚本）
reload.bat  # Windows
# 或
./reload.sh  # Linux/Mac

# 4. 测试
adb shell /data/local/tmp/kpm_control pengxintu123 help
adb shell /data/local/tmp/kpm_control pengxintu123 get_status

# 5. 查看日志
adb shell dmesg | grep "MyHook"
```

## 自动化脚本

如果你经常需要重新加载模块，可以创建一个开发脚本：

```bash
#!/bin/bash
# dev.sh - 开发辅助脚本

echo "Building..."
make || exit 1

echo "Reloading module..."
./reload.sh || exit 1

echo "Testing..."
adb shell /data/local/tmp/kpm_control pengxintu123 help

echo ""
echo "Ready for testing!"
```

使用：
```bash
chmod +x dev.sh
./dev.sh
```

## 注意事项

1. **总是先卸载再加载** - 不要尝试覆盖正在运行的模块
2. **等待卸载完成** - 卸载后等待 1-2 秒再加载新模块
3. **检查版本号** - 使用 `kpm info` 确认版本已更新
4. **测试新功能** - 使用 `help` 命令验证新功能可用
5. **查看日志** - 检查 dmesg 确认模块正确初始化

## 快速参考

| 操作 | 命令 |
|------|------|
| 卸载模块 | `adb shell su -c 'kpm unload kpm-inline-access'` |
| 加载模块 | `adb shell su -c 'kpm load /data/local/tmp/accessOffstinlineHook.kpm'` |
| 查看模块 | `adb shell su -c 'kpm list'` |
| 模块信息 | `adb shell su -c 'kpm info kpm-inline-access'` |
| 测试命令 | `adb shell /data/local/tmp/kpm_control pengxintu123 help` |
| 查看日志 | `adb shell dmesg \| grep "kpm-inline-access"` |
