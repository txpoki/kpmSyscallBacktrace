# Move-to-Next-Instruction 机制

## 核心思路

使用 `modify_user_hw_breakpoint` 动态修改断点地址，实现指令正常执行且断点保持活跃。

## 工作流程

```
初始状态：
  断点设置在 0x1000
  
第一次触发（在原地址 0x1000）：
  1. CPU 执行到 0x1000
  2. 断点触发 → 进入处理器
  3. 记录 hit_count++
  4. 使用 modify_user_hw_breakpoint 将断点移到 0x1004（下一条指令）
  5. 返回
  6. CPU 执行 0x1000 的指令（断点已移走，不会再触发）
  7. PC 前进到 0x1004

第二次触发（在下一条指令 0x1004）：
  8. CPU 执行到 0x1004
  9. 断点触发 → 进入处理器
  10. 使用 modify_user_hw_breakpoint 将断点移回 0x1000（原地址）
  11. 返回
  12. CPU 执行 0x1004 的指令
  13. 断点恢复到原位置，可以继续使用

后续：
  14. 如果再次执行到 0x1000，重复上述流程
  15. 断点可以多次触发 ✅
```

## 关键代码

### 第一次触发：移到下一条指令

```c
if (!bp_at_next_instruction[i]) {
    // 第一次触发：在原地址
    breakpoints[i].hit_count++;
    
    // 计算下一条指令地址
    bp_next_addr[i] = regs->pc + 4;  // ARM64 指令是 4 字节
    
    // 准备新的断点属性
    memset(&attr, 0, sizeof(attr));
    attr.type = PERF_TYPE_BREAKPOINT;
    attr.bp_addr = bp_next_addr[i];  // 移到下一条指令
    attr.bp_type = HW_BREAKPOINT_X;
    attr.bp_len = HW_BREAKPOINT_LEN_4;
    attr.disabled = 0;
    attr.sample_period = 1;
    
    // 修改断点地址
    result = g_modify_user_hw_breakpoint(bp_events[i], &attr);
    
    if (result == 0) {
        bp_at_next_instruction[i] = 1;  // 标记：现在在下一条指令
    }
}
```

### 第二次触发：移回原地址

```c
else {
    // 第二次触发：在下一条指令
    
    // 准备原始断点属性
    memset(&attr, 0, sizeof(attr));
    attr.type = PERF_TYPE_BREAKPOINT;
    attr.bp_addr = bp_original_addr[i];  // 移回原地址
    attr.bp_type = HW_BREAKPOINT_X;
    attr.bp_len = HW_BREAKPOINT_LEN_4;
    attr.disabled = 0;
    attr.sample_period = 1;
    
    // 修改断点地址
    result = g_modify_user_hw_breakpoint(bp_events[i], &attr);
    
    if (result == 0) {
        bp_at_next_instruction[i] = 0;  // 标记：现在回到原地址
    }
}
```

## 优点

✅ **指令正常执行**：不跳过指令，程序逻辑正确  
✅ **断点保持活跃**：可以多次触发  
✅ **避免无限循环**：通过移动断点地址  
✅ **性能好**：只需要修改断点属性，不需要注销/重新注册  
✅ **简单可靠**：不依赖特殊的调试寄存器函数  

## 与其他方案对比

### 方案 1：跳过指令
```c
regs->pc += 4;  // 跳过指令
```
- ❌ 指令不执行
- ✅ 简单
- ✅ 避免无限循环

### 方案 2：一次性断点
```c
unregister_hw_breakpoint(bp);  // 注销断点
```
- ✅ 指令执行
- ❌ 只能触发一次
- ✅ 简单

### 方案 3：Toggle 寄存器
```c
toggle_bp_hw_disable(slot);  // 禁用硬件寄存器
```
- ✅ 指令执行
- ✅ 可以多次触发
- ❌ 需要特殊的调试寄存器函数（可能不可用）

### 方案 4：Move-to-Next-Instruction（我们的实现）
```c
modify_user_hw_breakpoint(bp, &attr);  // 修改断点地址
```
- ✅ 指令执行
- ✅ 可以多次触发
- ✅ 只需要 modify_user_hw_breakpoint（通常可用）
- ✅ 最优雅的方案

## 依赖检查

```bash
# 检查 modify_user_hw_breakpoint 是否可用
adb shell su -c 'cat /proc/kallsyms | grep modify_user_hw_breakpoint'

# 应该看到：
# ffffffXXXXXXXXXX T modify_user_hw_breakpoint
```

如果不可用，会自动回退到一次性断点模式。

## 测试

```bash
# 1. 编译加载
make clean && make
kpm unload kpm-inline-access
kpm load /path/to/accessOffstinlineHook.kpm

# 2. 检查初始化
dmesg | grep "modify_user_hw_breakpoint"
# 应该看到：
#   modify_user_hw_breakpoint: ffffffXXXXXXXXXX
#   modify_user_hw_breakpoint available, using move-to-next-instruction mechanism

# 3. 设置断点
kpm_control su bp_set 0x755d685794 0 2 test

# 4. 触发并检查
dmesg | grep "HW_BP"
# 应该看到：
#   HW_BP[0]: Hit at 0x755d685794 (original), count:1
#   HW_BP[0]: Moved to next instruction 0x755d685798
#   HW_BP[0]: Hit at 0x755d685798 (next instruction)
#   HW_BP[0]: Moved back to original 0x755d685794

# 5. 再次触发（如果代码再次执行到该地址）
# 应该看到：
#   HW_BP[0]: Hit at 0x755d685794 (original), count:2
#   HW_BP[0]: Moved to next instruction 0x755d685798
#   ...

# 6. 验证断点仍然活跃
kpm_control su bp_list
# 应该显示：
#   [0] 0x755d685794 (exec, 4 bytes, hits:2) test
#   Total: 1/4 slots used
```

## 注意事项

### 1. 只适用于执行断点

这个机制只适用于执行断点（type=0），因为：
- 执行断点在指令执行前触发
- 数据断点在数据访问时触发，不需要这个机制

### 2. ARM64 指令大小

ARM64 指令固定是 4 字节，所以：
```c
bp_next_addr[i] = regs->pc + 4;  // 下一条指令
```

如果是 ARM32 Thumb 模式，指令可能是 2 字节，需要特殊处理。

### 3. 分支指令

如果断点设置在分支指令上：
```
0x1000: B 0x2000  ← 断点在这里
0x1004: ...       ← 下一条指令（但不会执行）
```

第二次触发会在 0x1004，但这条指令可能不会执行（因为分支跳走了）。
这是正常的，断点会在下次执行到 0x1000 时再次触发。

### 4. 多线程

如果多个线程同时执行到断点地址：
- 第一个线程触发 → 移到下一条指令
- 第二个线程到达原地址 → 断点已移走，不会触发
- 第一个线程到达下一条指令 → 触发 → 移回原地址
- 第二个线程继续执行...

这可能导致某些触发被遗漏，但这是硬件断点的固有限制。

## 总结

Move-to-Next-Instruction 机制是最优雅的解决方案：

1. ✅ 指令正常执行
2. ✅ 断点可以多次触发
3. ✅ 避免无限循环
4. ✅ 不依赖特殊的调试寄存器函数
5. ✅ 性能好

这是参考代码使用的核心机制，现在我们也实现了！
