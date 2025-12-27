# 常见问题 (FAQ)

## Q1: 为什么有时候符号解析失败？

**A**: 这是正常现象，主要原因：

1. **锁竞争** - 其他线程正在使用 `mmap_lock`，`trylock` 失败
2. **内存压力** - 临时缓冲区分配失败
3. **动态变化** - VMA 在查找和访问之间被修改

**示例**:
```
# 成功
[MyHook] #02 PC: 00000000ba90ac64 libUE4.so + 0x127fc64

# 失败（锁竞争）
[MyHook] #06 PC: 00000000ba900fe0
```

**解决方案**: 
- 这是设计权衡，使用 `trylock` 避免死锁
- 地址信息仍然完整，可以离线分析
- 改进后会显示 `<file> + offset` 作为降级

**详见**: [SYMBOL_RESOLUTION_ISSUES.md](SYMBOL_RESOLUTION_ISSUES.md)

---

## Q2: 日志太多怎么办？

**A**: `openat` 是高频系统调用，可以：

### 方法 1: 过滤特定进程
```bash
dmesg | grep "INLINE_OPENAT.*com.example.app"
```

### 方法 2: 过滤特定路径
```bash
dmesg | grep "INLINE_OPENAT.*/system/"
```

### 方法 3: 只看栈回溯
```bash
dmesg | grep -A 10 "INLINE_OPENAT.*target_app"
```

### 未来改进
- [ ] 添加进程过滤配置
- [ ] 添加路径过滤配置
- [ ] 添加采样率控制

---

## Q3: 如何离线分析地址？

**A**: 使用 `addr2line` 工具：

### 步骤 1: 提取地址
```bash
# 从日志提取
[MyHook] #02 PC: 00000000ba90ac64 libUE4.so + 0x127fc64
```

### 步骤 2: 获取库文件
```bash
adb pull /data/app/.../lib/arm64/libUE4.so
```

### 步骤 3: 使用 addr2line
```bash
aarch64-linux-gnu-addr2line -e libUE4.so -f -C 0x127fc64
```

### 或使用 IDA/Ghidra
1. 加载 libUE4.so
2. 跳转到偏移 0x127fc64
3. 查看函数名和源码位置

---

## Q4: 模块加载失败？

**A**: 检查以下几点：

### 1. KernelPatch 是否安装
```bash
kpm list
```

### 2. 符号是否存在
```bash
cat /proc/kallsyms | grep do_faccessat
cat /proc/kallsyms | grep do_sys_openat2
```

### 3. 查看详细错误
```bash
dmesg | tail -50
```

### 常见错误
- `do_faccessat missing` - 内核版本不支持
- `Hook installation failed` - 符号已被其他模块 Hook
- `Failed to initialize` - 依赖符号缺失

---

## Q5: 如何卸载模块？

**A**: 使用 kpm 命令：

```bash
# 卸载
kpm unload kpm-inline-access

# 验证
kpm list | grep inline-access
```

---

## Q6: 支持哪些内核版本？

**A**: 

### openat Hook
- ✅ Linux 5.6+ (`do_sys_openat2`)
- ✅ Linux 4.x - 5.5 (`do_sys_open`)
- ✅ 自动检测和回退

### faccessat Hook
- ✅ Linux 3.x+
- ✅ 大部分 Android 内核

### 架构支持
- ✅ ARM64
- ✅ ARM32 (兼容模式)

---

## Q7: 性能影响有多大？

**A**: 

### Hook 本身
- 极小（内联 Hook，几乎无开销）

### 栈回溯
- 中等（每次 100-500μs）
- 取决于栈深度

### 符号解析
- 较大（需要访问 VMA）
- 锁竞争时会跳过

### 总体影响
- 测试环境：可接受
- 生产环境：建议添加过滤

---

## Q8: 如何添加新的系统调用 Hook？

**A**: 参考 openat 的实现：

### 步骤 1: 添加全局变量
```c
static void *g_do_sys_xxx_addr = NULL;
```

### 步骤 2: 实现回调函数
```c
void before_do_sys_xxx(hook_fargs4_t *args, void *udata)
{
    // 处理参数
    // 获取进程信息
    // 执行栈回溯
}
```

### 步骤 3: 在 kpm_init 中安装 Hook
```c
g_do_sys_xxx_addr = kallsyms_lookup_name("do_sys_xxx");
if (g_do_sys_xxx_addr) {
    hook_wrap4(g_do_sys_xxx_addr, before_do_sys_xxx, NULL, 0);
}
```

### 步骤 4: 在 kpm_exit 中清理
```c
if (g_do_sys_xxx_addr) {
    unhook(g_do_sys_xxx_addr);
}
```

---

## Q9: 如何调试模块？

**A**: 

### 方法 1: 查看初始化日志
```bash
dmesg | grep -E "MyHook|StackUnwind|ProcessInfo"
```

### 方法 2: 测试触发
```bash
# 触发 openat
cat /system/build.prop

# 触发 faccessat
test -f /system/build.prop
```

### 方法 3: 检查符号
```bash
cat /proc/kallsyms | grep -E "do_faccessat|do_sys_openat2|save_stack_trace_user"
```

### 方法 4: 查看模块状态
```bash
kpm list
kpm info kpm-inline-access
```

---

## Q10: 如何贡献代码？

**A**: 

1. Fork 本项目
2. 创建特性分支
3. 提交更改
4. 创建 Pull Request

### 代码规范
- 遵循 Linux 内核编码风格
- 添加适当的注释
- 更新相关文档
- 测试编译和运行

---

## 更多问题？

- 查看 [SYMBOL_RESOLUTION_ISSUES.md](SYMBOL_RESOLUTION_ISSUES.md) - 符号解析问题
- 查看 [OPENAT_HOOK.md](OPENAT_HOOK.md) - openat Hook 详情
- 查看 [REFACTORING.md](REFACTORING.md) - 代码结构
- 查看 [QUICK_REFERENCE.md](QUICK_REFERENCE.md) - 快速参考

或提交 Issue 到项目仓库。
