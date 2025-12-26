# 快速参考

## 文件结构

```
kpms/demo_accessOffstinlineHook/
├── common.h                    # 共享类型定义
├── stack_unwind.h/c            # 栈回溯模块
├── process_info.h/c            # 进程信息模块
├── accessOffstinlineHook.c     # 主模块
├── Makefile                    # 构建配置
├── REFACTORING.md              # 重构说明
├── SUMMARY.md                  # 重构总结
└── test_build.sh               # 测试脚本
```

## 快速编译

```bash
export TARGET_COMPILE=aarch64-linux-gnu-
cd kpms/demo_accessOffstinlineHook
make clean && make
```

## 快速部署

```bash
make push
# 或
adb push accessOffstinlineHook.kpm /sdcard/kpm/
```

## 快速加载

```bash
kpm load /sdcard/kpm/accessOffstinlineHook.kpm
```

## 查看日志

```bash
# 实时查看
dmesg -w | grep -E "MyHook|StackUnwind|ProcessInfo"

# 查看历史
dmesg | grep -E "MyHook|StackUnwind|ProcessInfo"
```

## 模块接口

### stack_unwind 模块

```c
#include "stack_unwind.h"

// 初始化
int stack_unwind_init(void);

// 执行栈回溯
void unwind_user_stack_standard(struct task_struct *task);

// 获取 VMA 信息
int get_vma_info_str(unsigned long ip, char *buf, size_t len);

// 提取文件名
const char *my_kbasename(const char *path);
```

### process_info 模块

```c
#include "process_info.h"

// 初始化
int process_info_init(void);

// 获取进程 ID
pid_t get_process_id(struct task_struct *task);

// 获取进程命令行
void get_process_cmdline(struct task_struct *task, char *buf, size_t buf_len);
```

## 在其他模块中复用

### 示例：创建新的 Hook 模块

```c
#include <hook.h>
#include "common.h"
#include "stack_unwind.h"
#include "process_info.h"

void my_hook_callback(hook_fargs3_t *args, void *udata)
{
    struct task_struct *task = current;
    char cmdline[256];
    
    // 获取进程信息
    get_process_cmdline(task, cmdline, sizeof(cmdline));
    pr_info("Process: %s (PID:%d)\n", cmdline, get_process_id(task));
    
    // 执行栈回溯
    unwind_user_stack_standard(task);
}

static long kpm_init(const char *args, const char *event, void *reserved)
{
    // 初始化模块
    stack_unwind_init();
    process_info_init();
    
    // 安装 Hook
    void *target = kallsyms_lookup_name("your_target_function");
    hook_wrap3(target, my_hook_callback, NULL, 0);
    
    return 0;
}
```

## 常见问题

### Q: 编译失败？
A: 检查 `TARGET_COMPILE` 环境变量是否设置

### Q: 加载失败？
A: 检查 KernelPatch 是否已安装，使用 `kpm list` 验证

### Q: 没有日志输出？
A: 使用 `dmesg` 查看内核日志，确认模块已加载

### Q: 如何卸载模块？
A: `kpm unload kpm-inline-access`

## 相关文档

- **REFACTORING.md** - 详细的重构说明和原理
- **SUMMARY.md** - 重构成果总结
- **README.md** - 项目整体说明

## 技术支持

遇到问题？
1. 查看 `REFACTORING.md` 了解重构细节
2. 查看 `SUMMARY.md` 了解模块结构
3. 运行 `test_build.sh` 验证编译环境
