/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <compiler.h>
#include <kpmodule.h>
#include <hook.h> 
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <asm/current.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/stacktrace.h>
#include <linux/mm_types.h>
#include <linux/kernel.h>

#include <asm/ptrace.h>
#include <asm/processor.h>

#ifndef PSR_MODE32_BIT
#define PSR_MODE32_BIT 0x00000010
#endif

#define pr_fmt(fmt) "[MyHook] " fmt
KPM_NAME("kpm-inline-access");
KPM_VERSION("9.0.0");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("bmax121 & User");
KPM_DESCRIPTION("Inline Hook with print_vma_addr");


struct rw_semaphore;
struct pid_namespace;
struct file;

// 2. 函数指针定义
typedef void (*save_stack_trace_user_t)(struct stack_trace *trace);
typedef void (*get_task_comm_t)(char *buf, size_t buf_size, struct task_struct *tsk);
typedef pid_t (*task_pid_nr_ns_t)(struct task_struct *task, enum pid_type type, struct pid_namespace *ns);
typedef int (*get_cmdline_t)(struct task_struct *task, char *buffer, int buflen);
typedef long (*strncpy_from_user_t)(char *dst, const char __user *src, long count);
typedef unsigned long (*arch_copy_from_user_t)(void *to, const void __user *from, unsigned long n);

typedef void (*print_vma_addr_t)(char *prefix, unsigned long ip);

// VMA 相关函数指针
typedef struct vm_area_struct *(*find_vma_t)(struct mm_struct *mm, unsigned long addr);
typedef char *(*file_path_t)(struct file *, char *, int);
typedef int (*down_read_trylock_t)(struct rw_semaphore *sem);
typedef void (*up_read_t)(struct rw_semaphore *sem);
typedef void (*free_page_t)(unsigned long addr,unsigned int order);
typedef unsigned long (*get_free_page_t)(unsigned int gfp_mask, int order);
// 新增 snprintf 指针
typedef int (*snprintf_t)(char *buf, size_t size, const char *fmt, ...);

// 3. 全局函数指针变量
static find_vma_t          g_find_vma = NULL;
static file_path_t         g_file_path = NULL;
// static kbasename_t      g_kbasename = NULL; // [已移除]
static down_read_trylock_t g_down_read_trylock = NULL;
static up_read_t           g_up_read = NULL;
static free_page_t         g_free_page = NULL;
static get_free_page_t     g_get_free_page = NULL;
static snprintf_t          g_snprintf = NULL; // [新增]

static save_stack_trace_user_t g_save_stack_trace_user = NULL;
static get_task_comm_t         g_get_task_comm         = NULL;
static task_pid_nr_ns_t        g_task_pid_nr_ns        = NULL;
static get_cmdline_t           g_get_cmdline           = NULL;
static strncpy_from_user_t     g_strncpy_from_user     = NULL;
static print_vma_addr_t        g_print_vma_addr        = NULL; 
static arch_copy_from_user_t   g_arch_copy_from_user   = NULL;
static void * g_do_faccessat_addr     = NULL;

// 4. 偏移配置
struct vma_offsets_t {
    int vm_start;
    int vm_end;
    int vm_file;
    int vm_prev; // 新增
};

static struct vma_offsets_t g_vma_offset = {
    .vm_start = 0x00,
    .vm_end   = 0x08,
    .vm_prev  = 0x18,  // [新增] 标准 ARM64 偏移，通常在 vm_next(0x10) 之后
    .vm_file  = 0xA0
};

int g_mmap_lock_offset = 0x68;
static int g_task_mm_offset = 0x588;

// 5. 辅助函数实现

static inline long IS_ERR(const void *ptr)
{
    return (long)ptr >= (unsigned long)-4095;
}

// [实现 kbasename]
// 不依赖 strrchr，手动遍历查找最后一个 '/'
static const char *my_kbasename(const char *path)
{
    const char *tail = path;
    const char *p = path;
    
    if (!path) return NULL;
    
    while (*p) {
        if (*p == '/') {
            tail = p + 1;
        }
        p++;
    }
    return tail;
}

/**
 * 获取指定地址的 VMA 信息字符串
 */
static int get_vma_info_str(unsigned long ip, char *buf, size_t len)
{
  
    
    // 获取 current task
    uintptr_t current_task;
    asm volatile("mrs %0, sp_el0" : "=r"(current_task));
    struct mm_struct *mm = *(struct mm_struct **)(current_task + g_task_mm_offset);
    
    struct vm_area_struct *vma;
    struct rw_semaphore *mmap_sem;
    int ret_len = 0;

    if (!mm || !g_find_vma || !g_down_read_trylock || !g_up_read || !g_snprintf) {
        return 0;
    }

    mmap_sem = (struct rw_semaphore *)((char *)mm + g_mmap_lock_offset);

    if (!g_down_read_trylock(mmap_sem)) {
        return 0; 
    }

    // 查找当前 IP 所在的 VMA (例如代码段)
    vma = g_find_vma(mm, ip);

    if (vma) {
        struct file *f = *(struct file **)((char *)vma + g_vma_offset.vm_file);
        
        // 默认基址就是当前段的开始
        unsigned long base = *(unsigned long *)((char *)vma + g_vma_offset.vm_start);
        
        // --- [新增逻辑：回溯寻找 Base Address] ---
        if (f) {
            struct vm_area_struct *curr = vma;
            struct vm_area_struct *prev;
            struct file *prev_f;
            
            // 最多回溯 10 次，防止死循环或链表损坏
            for (int i = 0; i < 10; i++) {
                // 获取前一个 VMA
                prev = *(struct vm_area_struct **)((char *)curr + g_vma_offset.vm_prev);
                
                if (!prev) break; // 到头了
                
                // 检查前一个 VMA 的文件指针是否与当前一致
                // 注意：这里需要确保 prev 指针合法，内核里一般是合法的
                prev_f = *(struct file **)((char *)prev + g_vma_offset.vm_file);
                
                if (prev_f != f) {
                    // 文件变了，说明 current 已经是该文件的第一个段了
                    break;
                }
                
                // 这是一个属于同一个文件的更靠前的段，更新 base
                base = *(unsigned long *)((char *)prev + g_vma_offset.vm_start);
                curr = prev; // 继续往前找
            }
        }
        // ---------------------------------------

        // 计算基于文件基址的偏移 (RVA)
        unsigned long offset = ip - base;

        if (f) {
            if (g_file_path && g_get_free_page && g_free_page) {
                char *tmp_buf = (char *)g_get_free_page(0x400000, 0); 
                if (tmp_buf) {
                    char *p = g_file_path(f, tmp_buf, 4096);
                    if (IS_ERR(p)) p = "?";
                    const char *name = my_kbasename(p);
                    
                    // 打印格式：libc.so + 0xOffset
                    ret_len = g_snprintf(buf, len, " %s + 0x%lx", name, offset);
                    
                    g_free_page((unsigned long)tmp_buf,0);
                }
            }
        } else {
            // 匿名内存
            ret_len = g_snprintf(buf, len, " [anon] + 0x%lx", offset);
        }
    }

    g_up_read(mmap_sem);
    return ret_len;
}



static pid_t get_process_id(struct task_struct *task) {
    if (g_task_pid_nr_ns) return g_task_pid_nr_ns(task, PIDTYPE_TGID, NULL);
    return -1;
}

static void get_process_cmdline(struct task_struct *task, char *buf, size_t buf_len) {
    memset(buf, 0, buf_len);
    if (g_get_cmdline && g_get_cmdline(task, buf, buf_len) > 0) return;
    if (g_get_task_comm) g_get_task_comm(buf, buf_len, task);
    else strncpy(buf, "[Unknown]", buf_len);
}

struct stack_frame_32 {
    u32 next_fp;  
    u32 ret_addr;  
};

static void my_unwind_compat(struct task_struct *task, struct stack_trace *trace)
{
    struct pt_regs *regs = task_pt_regs(task);
    if (!regs) return;

    // 1. 获取基本寄存器
    u32 sp = (u32)regs->regs[13];
    if (sp == 0) sp = (u32)regs->sp;

    u32 lr = (u32)regs->regs[14];
    u32 pc = (u32)regs->pc;

    // 2. 记录当前执行点 (PC)
    if (trace->nr_entries < trace->max_entries) {
        trace->entries[trace->nr_entries++] = pc;
    }

    // 3. 记录返回地址 (LR) - 针对叶子函数
    if (trace->nr_entries < trace->max_entries && lr > 0x1000) {
        trace->entries[trace->nr_entries++] = lr;
    }

    // 4. 获取起始 Frame Pointer (FP)
    u32 fp = 0;
    // 检查 CPSR/PSTATE 的 T 位 (Bit 5)，判断是否为 Thumb 模式
    bool is_thumb = (regs->pstate & 0x20) != 0;

    if (is_thumb) {
        // Thumb 模式标准：FP = R7
        fp = (u32)regs->regs[7];
    } else {
        // ARM 模式标准：FP = R11
        fp = (u32)regs->regs[11];
    }

    // 5. 循环回溯栈帧
    int depth = 0;
    while (trace->nr_entries < trace->max_entries && depth < 32) {
        struct stack_frame_32 frame;

        // 5.1 合法性检查
        // FP 必须: >0x1000(避开空指针), <0xffff...(用户空间), 4字节对齐, >=SP(栈向下生长)
        if (fp < 0x1000 || fp > 0xfffffff0 || (fp & 3) || fp < sp) {
            break;
        }

        // 5.2 读取栈帧内存
        if (g_arch_copy_from_user) {
            // 如果读取失败（例如读到了栈底 0 地址或者缺页），直接 break
            if (g_arch_copy_from_user(&frame, (const void __user *)(unsigned long)fp, sizeof(frame)) != 0) {
                break;
            }
        } else {
            break;
        }

        // 5.3 记录返回地址
        if (frame.ret_addr > 0x1000) {
            // 去重：如果解出来的地址和上一条(LR)一样，就不存了
            if (trace->nr_entries > 0 && trace->entries[trace->nr_entries - 1] != frame.ret_addr) {
                trace->entries[trace->nr_entries++] = frame.ret_addr;
            }
        }

        // 5.4 移动指针 (关键终止条件)
        // 栈底的 next_fp 通常为 0，或者非法值。
        // 正常的栈帧链表，地址应该越来越高 (stack grows down, addresses grow up)
        if (frame.next_fp <= fp) {
            
            break;
        }

        fp = frame.next_fp;
        depth++;
    }


    if (trace->nr_entries < trace->max_entries) {
        trace->entries[trace->nr_entries++] = (unsigned long)-1; // 即 0xFFFFFFFFFFFFFFFF
    }
}

#define MAX_STACK_DEPTH 32

void unwind_user_stack_standard(struct task_struct *task)
{
    unsigned long stack_entries[MAX_STACK_DEPTH];
    struct stack_trace trace;
    char vma_info_buf[256];

    if (!g_save_stack_trace_user) return;

    memset(&trace, 0, sizeof(trace));
    trace.nr_entries = 0;
    trace.max_entries = MAX_STACK_DEPTH;
    trace.entries = stack_entries;
    trace.skip = 0;

    struct pt_regs *regs = task_pt_regs(task);
    bool is_32bit = false;
    if (regs && (regs->pstate & PSR_MODE32_BIT)) {
        is_32bit = true;
    }
    
    if (is_32bit) {
        my_unwind_compat(task, &trace);
    } else {
        if (g_save_stack_trace_user) {
            g_save_stack_trace_user(&trace);
        }
    }    
    
    for (int i = 0; i < trace.nr_entries; i++) {
        unsigned long ip = trace.entries[i];
        
        vma_info_buf[0] = '\0';
        // 调用封装好的函数获取 VMA 信息
        get_vma_info_str(ip, vma_info_buf, sizeof(vma_info_buf));
        
        // 单行打印
        pr_info("#%02d PC: %016lx%s\n", i, ip, vma_info_buf);
    }
    
    pr_info("------------------------------------------\n");
}

void before_do_faccessat(hook_fargs3_t *args, void *udata)
{
    const char __user *filename = (const char __user *)args->arg1;
    int mode = (int)args->arg2;

    char path_buf[256] = {0};
    char pkg_name[256] = {0};

    if (g_strncpy_from_user) {
        long ret = g_strncpy_from_user(path_buf, filename, sizeof(path_buf) - 1);
        if (ret < 0) strncpy(path_buf, "<read_error>", sizeof(path_buf));
    } else {
        strncpy(path_buf, "<symbol_missing>", sizeof(path_buf));
    }

    struct task_struct *task = current;
    get_process_cmdline(task, pkg_name, sizeof(pkg_name));

    pr_info("INLINE_ACCESS: [%s] (PID:%d) -> %s [Mode:%d]\n", 
            pkg_name, get_process_id(task), path_buf, mode);

    unwind_user_stack_standard(task);
}

static long kpm_init(const char *args, const char *event, void *__user reserved)
{
    pr_info("kpm-inline-access (v9.0 print_vma_addr) init...\n");

    g_save_stack_trace_user = (save_stack_trace_user_t)kallsyms_lookup_name("save_stack_trace_user");
    g_get_task_comm         = (get_task_comm_t)kallsyms_lookup_name("__get_task_comm");
    g_task_pid_nr_ns        = (task_pid_nr_ns_t)kallsyms_lookup_name("__task_pid_nr_ns");
    g_get_cmdline           = (get_cmdline_t)kallsyms_lookup_name("get_cmdline");
    g_strncpy_from_user     = (strncpy_from_user_t)kallsyms_lookup_name("strncpy_from_user");
    g_print_vma_addr        = (print_vma_addr_t)kallsyms_lookup_name("print_vma_addr");

    if (!g_save_stack_trace_user) { pr_err("save_stack_trace_user missing\n"); return -1; }

    g_do_faccessat_addr = (void *)kallsyms_lookup_name("do_faccessat");
    if (!g_do_faccessat_addr) { pr_err("do_faccessat missing\n"); return -1; }

    g_arch_copy_from_user = (arch_copy_from_user_t)kallsyms_lookup_name("__arch_copy_from_user");
    if (!g_arch_copy_from_user) {
        pr_err("copy_from_user symbol missing!\n");
        return -1;
    }

    // VMA 相关符号查找
    g_find_vma = (find_vma_t)kallsyms_lookup_name("find_vma");
    g_file_path = (file_path_t)kallsyms_lookup_name("file_path");
    if (!g_file_path) g_file_path = (file_path_t)kallsyms_lookup_name("d_path");
    
    // g_kbasename 移除，使用本地实现

    g_down_read_trylock = (down_read_trylock_t)kallsyms_lookup_name("down_read_trylock");
    g_up_read = (up_read_t)kallsyms_lookup_name("up_read");
    g_get_free_page = (get_free_page_t)kallsyms_lookup_name("__get_free_pages"); 
    g_free_page = (free_page_t)kallsyms_lookup_name("free_pages");
    g_snprintf = (snprintf_t)kallsyms_lookup_name("snprintf"); // [新增]

    if (!g_find_vma || !g_file_path || !g_down_read_trylock || !g_snprintf) {
        pr_warn("VMA/snprintf symbols missing, logging functionality restricted.\n");
        // return -1; // 可选：如果只是日志功能受限，可以不返回错误
    }

    hook_err_t err = hook_wrap3(g_do_faccessat_addr, before_do_faccessat, NULL, 0);
    if (err) return err;

    pr_info("Hook success.\n");
    return 0;
}

static long kpm_exit(void *__user reserved)
{
    if (g_do_faccessat_addr) unhook(g_do_faccessat_addr);
    return 0;
}

KPM_INIT(kpm_init);
KPM_EXIT(kpm_exit);