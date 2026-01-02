# 中断上下文编程规则

## 什么是中断上下文？

硬件断点处理器、IRQ 处理器等在**中断上下文**中运行。这是一个高度受限的执行环境。

## 黄金规则

### ❌ 绝对禁止

1. **睡眠或阻塞**
   ```c
   // ❌ 禁止
   msleep(100);
   schedule();
   wait_event(...);
   ```

2. **获取可能阻塞的锁**
   ```c
   // ❌ 禁止
   mutex_lock(&my_mutex);
   down(&my_semaphore);
   ```

3. **访问用户空间**
   ```c
   // ❌ 禁止
   copy_from_user(...);
   copy_to_user(...);
   get_user(...);
   ```

4. **调用可能睡眠的内核函数**
   ```c
   // ❌ 禁止
   kmalloc(..., GFP_KERNEL);  // 只能用 GFP_ATOMIC
   __get_task_comm(...);       // 可能获取锁
   get_cmdline(...);           // 可能睡眠
   ```

5. **复杂的内存操作**
   ```c
   // ❌ 禁止
   vmalloc(...);
   kzalloc(large_size, GFP_KERNEL);
   ```

### ✅ 允许的操作

1. **简单的变量操作**
   ```c
   // ✅ 允许
   counter++;
   flag = 1;
   data[i] = value;
   ```

2. **自旋锁（短时间持有）**
   ```c
   // ✅ 允许（但要快）
   spin_lock_irqsave(&my_lock, flags);
   // ... 快速操作 ...
   spin_unlock_irqrestore(&my_lock, flags);
   ```

3. **原子操作**
   ```c
   // ✅ 允许
   atomic_inc(&counter);
   atomic_set(&flag, 1);
   ```

4. **简短的日志**
   ```c
   // ✅ 允许（但不要太频繁）
   pr_info("Event occurred: %d\n", value);
   ```

5. **GFP_ATOMIC 内存分配**
   ```c
   // ✅ 允许（小内存）
   ptr = kmalloc(sizeof(struct small_struct), GFP_ATOMIC);
   ```

6. **调度工作队列**
   ```c
   // ✅ 允许
   queue_work(my_wq, &my_work);
   schedule_work(&my_work);
   ```

## 硬件断点处理器的正确模式

### ❌ 错误示例

```c
static void hw_bp_handler(...)
{
    struct task_struct *task = current;
    char name[256];
    
    // ❌ 调用可能获取锁的函数
    get_process_cmdline(task, name, sizeof(name));
    
    // ❌ 调用可能访问用户空间的函数
    unwind_user_stack(task);
    
    // ❌ 复杂的处理
    pr_info("Process: %s, Stack: ...\n", name);
}
```

**结果：** 程序冻结，死锁，或崩溃

### ✅ 正确示例（最小模式）

```c
static void hw_bp_handler(...)
{
    int i;
    
    // 找到触发的断点
    for (i = 0; i < MAX_BREAKPOINTS; i++) {
        if (bp_events[i] == bp && breakpoints[i].enabled) {
            // ✅ 简单的计数器操作
            breakpoints[i].hit_count++;
            
            // ✅ 简短的日志，只用静态数据
            pr_info("HW_BP[%d]: Hit at 0x%lx, count:%d\n",
                    i, breakpoints[i].addr, breakpoints[i].hit_count);
            
            break;
        }
    }
    // ✅ 立即返回
}
```

**结果：** 安全，快速，可靠

### ✅ 正确示例（带工作队列）

```c
static void hw_bp_handler(...)
{
    // 在中断上下文中：最小工作
    breakpoints[i].hit_count++;
    pr_info("HW_BP[%d]: Hit\n", i);
    
    // ✅ 调度延迟工作
    if (verbose_mode) {
        struct work_item *work = kmalloc(sizeof(*work), GFP_ATOMIC);
        if (work) {
            work->index = i;
            work->addr = breakpoints[i].addr;
            INIT_WORK(&work->work, deferred_handler);
            queue_work(my_wq, &work->work);
        }
    }
}

// 这个在进程上下文中运行
static void deferred_handler(struct work_struct *work)
{
    // ✅ 这里可以做复杂操作
    get_process_info(...);
    unwind_stack(...);
    pr_info("Detailed info: ...\n");
}
```

**结果：** 安全 + 详细信息

## 如何判断函数是否安全？

### 检查清单

1. **查看函数实现**
   - 是否调用 `schedule()`？
   - 是否使用 `mutex_lock()`？
   - 是否访问用户空间？

2. **查看内核文档**
   - 函数是否标记为 "may sleep"？
   - 是否要求进程上下文？

3. **测试**
   - 启用 `CONFIG_DEBUG_ATOMIC_SLEEP`
   - 内核会警告不安全的操作

### 常见函数的安全性

| 函数 | 中断上下文 | 说明 |
|------|-----------|------|
| `pr_info()` | ✅ 安全 | 简短使用 |
| `printk()` | ✅ 安全 | 简短使用 |
| `kmalloc(GFP_ATOMIC)` | ✅ 安全 | 小内存 |
| `kmalloc(GFP_KERNEL)` | ❌ 不安全 | 可能睡眠 |
| `spin_lock()` | ✅ 安全 | 快速持有 |
| `mutex_lock()` | ❌ 不安全 | 可能睡眠 |
| `get_task_comm()` | ⚠️ 取决于实现 | 可能获取锁 |
| `__task_pid_nr_ns()` | ⚠️ 取决于实现 | 可能获取锁 |
| `copy_from_user()` | ❌ 不安全 | 访问用户空间 |
| `queue_work()` | ✅ 安全 | 推荐方式 |

## 调试技巧

### 1. 启用内核调试选项

```
CONFIG_DEBUG_ATOMIC_SLEEP=y
CONFIG_PROVE_LOCKING=y
CONFIG_DEBUG_SPINLOCK=y
```

### 2. 检查内核日志

```bash
dmesg | grep -i "sleep\|lock\|atomic"
```

### 3. 使用 `in_interrupt()` 检查

```c
if (in_interrupt()) {
    pr_info("Running in interrupt context\n");
    // 只做安全操作
} else {
    pr_info("Running in process context\n");
    // 可以做复杂操作
}
```

## 总结

### 中断上下文的三个原则

1. **快速**：尽快完成并返回
2. **简单**：只做最少的必要工作
3. **安全**：不调用可能阻塞的函数

### 需要复杂处理时

使用**工作队列**将工作延迟到进程上下文：

```
中断上下文 (快速)
    ↓
  调度工作项
    ↓
进程上下文 (可以做复杂操作)
```

### 记住

> **在中断上下文中，如果不确定某个操作是否安全，就不要做！**

## 参考资料

- Linux Kernel Documentation: Interrupt Context
- "Linux Device Drivers" by Rubini & Corbet
- Kernel source: `Documentation/kernel-hacking/locking.rst`
