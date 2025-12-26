# 代码重构说明

## 重构目标

将 `accessOffstinlineHook.c` 中的栈回溯和进程信息相关逻辑解耦到独立文件，保持代码逻辑完全不变。

## 文件结构

### 新增文件

1. **common.h** - 共享的类型定义和头文件
   - 所有模块共用的类型定义
   - 函数指针类型定义
   - 结构体定义
   - 前向声明

2. **stack_unwind.h / stack_unwind.c** - 栈回溯模块
   - `stack_unwind_init()` - 初始化栈回溯模块
   - `unwind_user_stack_standard()` - 执行栈回溯
   - `get_vma_info_str()` - 获取 VMA 信息
   - `my_kbasename()` - 路径基名提取
   - `my_unwind_compat()` - ARM32 栈回溯（内部函数）

3. **process_info.h / process_info.c** - 进程信息模块
   - `process_info_init()` - 初始化进程信息模块
   - `get_process_id()` - 获取进程 ID
   - `get_process_cmdline()` - 获取进程命令行

4. **accessOffstinlineHook.c** - 主模块（重构后）
   - Hook 回调函数
   - 模块初始化和清理
   - 协调子模块工作

## 重构原则

1. **保持逻辑不变** - 所有函数的实现逻辑完全保持原样
2. **保持类型不变** - 所有类型定义、函数签名完全一致
3. **保持行为不变** - 编译后的行为与原代码完全相同
4. **物理分离** - 只是将代码从一个文件分离到多个文件

## 代码对比

### 原始结构（单文件）
```
accessOffstinlineHook.c (450+ 行)
├── 头文件包含
├── 类型定义
├── 全局变量
├── 辅助函数
│   ├── IS_ERR
│   ├── my_kbasename
│   ├── get_vma_info_str
│   ├── get_process_id
│   ├── get_process_cmdline
│   ├── my_unwind_compat
│   └── unwind_user_stack_standard
├── Hook 回调函数
└── 模块初始化/清理
```

### 重构后结构（多文件）
```
common.h (共享定义)
├── 头文件包含
├── 类型定义
├── 结构体定义
└── 内联函数

stack_unwind.c/h (栈回溯)
├── my_kbasename
├── get_vma_info_str
├── my_unwind_compat
├── unwind_user_stack_standard
└── stack_unwind_init

process_info.c/h (进程信息)
├── get_process_id
├── get_process_cmdline
└── process_info_init

accessOffstinlineHook.c (主模块)
├── Hook 回调函数
├── kpm_init
└── kpm_exit
```

## 编译

```bash
export TARGET_COMPILE=aarch64-linux-gnu-
cd kpms/demo_accessOffstinlineHook
make clean
make
```

编译会生成：
- `accessOffstinlineHook.o` - 主模块对象文件
- `stack_unwind.o` - 栈回溯模块对象文件
- `process_info.o` - 进程信息模块对象文件
- `accessOffstinlineHook.kpm` - 最终模块文件

## 使用

使用方式与重构前完全相同：

```bash
kpm load /sdcard/kpm/accessOffstinlineHook.kpm
dmesg | grep -E "MyHook|StackUnwind|ProcessInfo"
```

## 优势

1. **模块化** - 功能清晰分离，职责明确
2. **可维护** - 每个模块独立维护，易于理解
3. **可复用** - 栈回溯和进程信息模块可被其他 KPM 复用
4. **可测试** - 每个模块可独立测试
5. **向后兼容** - 编译结果和行为完全一致

## 注意事项

1. 所有模块共享 `common.h` 中的类型定义
2. 每个模块有自己的 `pr_fmt` 定义，便于日志区分
3. 全局变量在各自模块中定义为 `static`，避免符号冲突
4. 函数签名保持完全一致，确保兼容性
