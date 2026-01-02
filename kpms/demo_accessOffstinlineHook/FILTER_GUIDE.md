# 过滤功能使用指南

## 功能概述

模块现在支持灵活的过滤功能，可以：
- 按包名/进程名过滤
- 按 PID 过滤
- 选择性启用/禁用特定系统调用的 hook
- 支持白名单和黑名单两种模式

## 过滤模式

### 白名单模式 (Whitelist)
- **行为**: 只 hook 匹配过滤器的进程
- **用途**: 专注监控特定应用
- **默认**: 如果没有过滤器，不 hook 任何进程

### 黑名单模式 (Blacklist)
- **行为**: hook 所有进程，除了匹配过滤器的
- **用途**: 排除不需要监控的进程
- **默认**: 如果没有过滤器，hook 所有进程

## 基本命令

### 查看状态
```bash
./kpm_control <key> get_status
```

输出示例：
```
enabled=1
access_hook=1
openat_hook=1
access_count=123
openat_count=456
total_hooks=579
filter_mode=whitelist
filter_count=2
filter[0]=name:com.example.app
filter[1]=pid:1234
```

### 启用/禁用所有 hooks
```bash
./kpm_control <key> enable    # 启用所有
./kpm_control <key> disable   # 禁用所有
```

### 控制特定系统调用
```bash
# Access hook
./kpm_control <key> enable_access
./kpm_control <key> disable_access

# Openat hook
./kpm_control <key> enable_openat
./kpm_control <key> disable_openat
```

## 过滤器管理

### 设置过滤模式
```bash
# 白名单模式（只 hook 匹配的）
./kpm_control <key> set_whitelist

# 黑名单模式（排除匹配的）
./kpm_control <key> set_blacklist
```

### 添加过滤器

#### 按包名/进程名过滤
```bash
./kpm_control <key> add_name com.example.app
./kpm_control <key> add_name system_server
./kpm_control <key> add_name zygote
```

**注意**: 使用部分匹配，例如 "example" 会匹配 "com.example.app"

#### 按 PID 过滤
```bash
./kpm_control <key> add_pid 1234
./kpm_control <key> add_pid 5678
```

### 清除所有过滤器
```bash
./kpm_control <key> clear_filters
```

## 使用场景

### 场景 1: 只监控特定应用

```bash
# 1. 设置白名单模式
./kpm_control pengxintu123 set_whitelist

# 2. 添加要监控的应用
./kpm_control pengxintu123 add_name com.tencent.mm
./kpm_control pengxintu123 add_name com.tencent.mobileqq

# 3. 查看状态
./kpm_control pengxintu123 get_status

# 现在只会 hook 微信和 QQ
```

### 场景 2: 排除系统进程

```bash
# 1. 设置黑名单模式
./kpm_control pengxintu123 set_blacklist

# 2. 添加要排除的进程
./kpm_control pengxintu123 add_name system_server
./kpm_control pengxintu123 add_name surfaceflinger
./kpm_control pengxintu123 add_name zygote

# 3. 查看状态
./kpm_control pengxintu123 get_status

# 现在会 hook 所有进程，除了系统进程
```

### 场景 3: 只监控 openat，不监控 access

```bash
# 1. 禁用 access hook
./kpm_control pengxintu123 disable_access

# 2. 确保 openat hook 启用
./kpm_control pengxintu123 enable_openat

# 3. 查看状态
./kpm_control pengxintu123 get_status
```

### 场景 4: 监控特定 PID

```bash
# 1. 设置白名单模式
./kpm_control pengxintu123 set_whitelist

# 2. 添加 PID（例如从 ps 命令获取）
./kpm_control pengxintu123 add_pid 12345

# 3. 查看状态
./kpm_control pengxintu123 get_status

# 现在只会 hook PID 12345 的进程
```

### 场景 5: 临时禁用然后恢复

```bash
# 1. 禁用所有 hooks
./kpm_control pengxintu123 disable

# 2. 做一些操作...

# 3. 重新启用
./kpm_control pengxintu123 enable

# 过滤器配置保持不变
```

## 完整工作流程示例

### 监控游戏应用的文件访问

```bash
# 1. 设置白名单模式
./kpm_control pengxintu123 set_whitelist

# 2. 添加游戏包名
./kpm_control pengxintu123 add_name com.tencent.tmgp.sgame

# 3. 只监控 openat（文件打开），不监控 access
./kpm_control pengxintu123 disable_access
./kpm_control pengxintu123 enable_openat

# 4. 重置计数器
./kpm_control pengxintu123 reset_counters

# 5. 启动游戏并玩一会儿

# 6. 查看统计
./kpm_control pengxintu123 get_status

# 7. 查看内核日志
adb shell dmesg | grep "INLINE_OPENAT.*sgame"

# 8. 完成后清除过滤器
./kpm_control pengxintu123 clear_filters
```

## 过滤器限制

- 最多支持 16 个过滤器
- 包名/进程名最长 256 字符
- 使用部分匹配（strstr）
- PID 必须是正整数

## 性能考虑

### 最佳实践

1. **使用白名单模式监控少数应用**
   - 性能影响最小
   - 日志量可控

2. **禁用不需要的 hook**
   - 如果只关心文件打开，禁用 access hook
   - 减少不必要的处理

3. **避免过于宽泛的过滤器**
   - 不要使用太短的名称（如 "app"）
   - 可能匹配太多进程

4. **定期重置计数器**
   - 避免计数器溢出
   - 便于统计特定时间段的数据

## 故障排除

### 过滤器不生效

```bash
# 1. 检查过滤模式
./kpm_control pengxintu123 get_status

# 2. 确认过滤器已添加
# 输出应该显示 filter[0]=name:xxx

# 3. 确认 hooks 已启用
# enabled=1, access_hook=1, openat_hook=1

# 4. 检查进程名是否正确
adb shell ps | grep <package_name>
```

### 日志太多

```bash
# 方案 1: 使用白名单模式
./kpm_control pengxintu123 set_whitelist
./kpm_control pengxintu123 add_name <specific_app>

# 方案 2: 禁用某个 hook
./kpm_control pengxintu123 disable_access

# 方案 3: 临时禁用
./kpm_control pengxintu123 disable
```

### 找不到进程

```bash
# 1. 列出所有进程
adb shell ps -A

# 2. 查找特定应用
adb shell ps -A | grep <keyword>

# 3. 获取 PID
adb shell pidof <package_name>

# 4. 使用 PID 过滤
./kpm_control pengxintu123 add_pid <pid>
```

## 高级用法

### 组合过滤器

```bash
# 同时按名称和 PID 过滤
./kpm_control pengxintu123 set_whitelist
./kpm_control pengxintu123 add_name com.example.app
./kpm_control pengxintu123 add_pid 1234

# 任何一个匹配就会 hook
```

### 动态调整

```bash
# 运行时添加新过滤器
./kpm_control pengxintu123 add_name new.app

# 不需要重启模块或清除现有过滤器
```

### 监控脚本

```bash
#!/system/bin/sh
# monitor.sh - 自动监控脚本

KEY="pengxintu123"

# 设置
./kpm_control $KEY set_whitelist
./kpm_control $KEY add_name com.target.app
./kpm_control $KEY reset_counters

echo "Monitoring started. Press Ctrl+C to stop."

# 每 5 秒显示统计
while true; do
    sleep 5
    clear
    ./kpm_control $KEY get_status
    echo ""
    echo "Recent logs:"
    dmesg | grep "INLINE_" | tail -10
done
```

## 命令速查表

| 命令 | 说明 |
|------|------|
| `get_status` | 查看状态 |
| `enable` / `disable` | 启用/禁用所有 |
| `enable_access` / `disable_access` | 控制 access hook |
| `enable_openat` / `disable_openat` | 控制 openat hook |
| `set_whitelist` | 白名单模式 |
| `set_blacklist` | 黑名单模式 |
| `add_name <name>` | 添加名称过滤器 |
| `add_pid <pid>` | 添加 PID 过滤器 |
| `clear_filters` | 清除所有过滤器 |
| `reset_counters` | 重置计数器 |
| `help` | 显示帮助 |
