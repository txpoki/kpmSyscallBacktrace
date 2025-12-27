# OpenAt Hook 功能说明

## 新增功能

在原有的 `do_faccessat` Hook 基础上，新增了对 `openat` 系统调用的 Hook 支持。

## Hook 目标

### 主要目标：do_sys_openat2
- **函数**: `do_sys_openat2`
- **内核版本**: Linux 5.6+
- **用途**: 现代内核中 `openat` 和 `openat2` 系统调用的底层实现

### 备用目标：do_sys_open
- **函数**: `do_sys_open`
- **内核版本**: 较旧的内核版本
- **用途**: 旧版内核中 `open` 和 `openat` 系统调用的底层实现

## 实现细节

### Hook 回调函数

```c
void before_do_sys_openat2(hook_fargs4_t *args, void *udata)
{
    int dfd = (int)args->arg0;                      // 目录文件描述符
    const char __user *filename = (const char __user *)args->arg1;  // 文件路径
    // arg2 是 struct open_how *how (打开方式)
    // arg3 是 size_t size (结构体大小)
    
    // 读取文件路径
    // 获取进程信息
    // 打印日志
    // 执行栈回溯
}
```

### 初始化逻辑

```c
// 1. 尝试 Hook do_sys_openat2 (新内核)
g_do_sys_openat2_addr = kallsyms_lookup_name("do_sys_openat2");
if (g_do_sys_openat2_addr) {
    hook_wrap4(g_do_sys_openat2_addr, before_do_sys_openat2, NULL, 0);
}

// 2. 如果失败，尝试 Hook do_sys_open (旧内核)
if (!g_do_sys_openat2_addr) {
    g_do_sys_openat2_addr = kallsyms_lookup_name("do_sys_open");
    if (g_do_sys_openat2_addr) {
        hook_wrap4(g_do_sys_openat2_addr, before_do_sys_openat2, NULL, 0);
    }
}
```

## 输出示例

### 加载模块时
```
[MyHook] kpm-inline-access (v9.0 print_vma_addr) init...
[StackUnwind] Initializing...
[ProcessInfo] Initializing...
[MyHook] do_faccessat hook installed
[MyHook] do_sys_openat2 hook installed
[MyHook] Hook initialization complete
```

### 文件打开时
```
[MyHook] INLINE_OPENAT: [com.android.systemui] (PID:1234) -> /system/framework/services.jar [DFD:-100]
[StackUnwind] #00 PC: 00007b2c4a8f20 libc.so + 0x2f20
[StackUnwind] #01 PC: 00007b2c4a8f40 libc.so + 0x2f40
[StackUnwind] #02 PC: 00007b2c5a1234 libandroid_runtime.so + 0x1234
[StackUnwind] ------------------------------------------
```

### 文件访问时（原有功能）
```
[MyHook] INLINE_ACCESS: [com.android.systemui] (PID:1234) -> /data/data/com.example/file.txt [Mode:0]
[StackUnwind] #00 PC: 00007b2c4a8f20 libc.so + 0x2f20
[StackUnwind] ------------------------------------------
```

## 参数说明

### DFD (Directory File Descriptor)
- **-100 (AT_FDCWD)**: 相对于当前工作目录
- **>= 0**: 相对于指定的目录文件描述符
- 用于实现相对路径打开

### 日志前缀
- **INLINE_ACCESS**: `faccessat` 系统调用
- **INLINE_OPENAT**: `openat/openat2` 系统调用

## 兼容性

### 支持的内核版本
- ✅ Linux 5.6+ (使用 `do_sys_openat2`)
- ✅ Linux 4.x - 5.5 (使用 `do_sys_open`)
- ✅ 自动检测和回退机制

### 架构支持
- ✅ ARM64
- ✅ ARM32 (通过兼容模式)

## 使用场景

### 1. 文件访问监控
监控应用程序打开的所有文件：
```bash
dmesg -w | grep "INLINE_OPENAT"
```

### 2. 安全审计
追踪敏感文件的访问：
```bash
dmesg | grep "INLINE_OPENAT.*sensitive_file"
```

### 3. 调试分析
分析应用的文件访问模式和调用栈：
```bash
dmesg | grep -A 5 "INLINE_OPENAT.*com.example.app"
```

## 性能考虑

### 日志频率
`openat` 是非常高频的系统调用，可能产生大量日志。建议：
1. 在测试环境使用
2. 添加进程过滤（未来功能）
3. 添加路径过滤（未来功能）

### 性能影响
- Hook 本身：极小（内联 Hook）
- 栈回溯：中等（每次调用约 100-500μs）
- 日志输出：较大（取决于日志系统）

## 未来改进

### 短期
- [ ] 添加进程名过滤
- [ ] 添加路径过滤（如只监控特定目录）
- [ ] 添加打开标志解析（O_RDONLY, O_WRONLY 等）

### 中期
- [ ] 支持更多文件操作系统调用（read, write, close）
- [ ] 添加文件操作统计
- [ ] 实现用户态配置接口

### 长期
- [ ] 文件访问行为分析
- [ ] 异常访问检测
- [ ] 与用户态工具集成

## 故障排除

### Hook 安装失败
```
[MyHook] do_sys_openat2 hook installation failed, trying do_sys_open
[MyHook] do_sys_open hook installation failed
```
**原因**: 内核符号不存在或已被其他模块 Hook
**解决**: 检查内核版本和符号表

### 没有日志输出
**原因**: 
1. 模块未正确加载
2. 日志级别过滤
3. 没有文件打开操作

**解决**:
```bash
# 检查模块状态
kpm list | grep inline-access

# 查看所有日志
dmesg | tail -100

# 测试触发
cat /system/build.prop
```

## 相关文件

- `accessOffstinlineHook.c` - 主模块实现
- `REFACTORING.md` - 模块重构说明
- `SUMMARY.md` - 项目总结
- `QUICK_REFERENCE.md` - 快速参考

## 参考资料

- Linux Kernel Source: `fs/open.c`
- `openat(2)` man page
- `openat2(2)` man page
