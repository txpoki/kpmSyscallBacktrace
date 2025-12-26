/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Stack unwinding implementation
 */

#include "stack_unwind.h"



// Global function pointers
static save_stack_trace_user_t g_save_stack_trace_user = NULL;
static arch_copy_from_user_t g_arch_copy_from_user = NULL;
static find_vma_t g_find_vma = NULL;
static file_path_t g_file_path = NULL;
static down_read_trylock_t g_down_read_trylock = NULL;
static up_read_t g_up_read = NULL;
static free_page_t g_free_page = NULL;
static get_free_page_t g_get_free_page = NULL;
static snprintf_t g_snprintf = NULL;

// VMA offset configuration
static struct vma_offsets_t g_vma_offset = {
    .vm_start = 0x00,
    .vm_end   = 0x08,
    .vm_prev  = 0x18,
    .vm_file  = 0xA0
};

static int g_mmap_lock_offset = 0x68;
static int g_task_mm_offset = 0x588;

// [实现 kbasename]
// 不依赖 strrchr，手动遍历查找最后一个 '/'
const char *my_kbasename(const char *path)
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
int get_vma_info_str(unsigned long ip, char *buf, size_t len)
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
                    
                    g_free_page((unsigned long)tmp_buf, 0);
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
        if (fp < 0x1000 || fp > 0xfffffff0 || (fp & 3) || fp < sp) {
            break;
        }

        // 5.2 读取栈帧内存
        if (g_arch_copy_from_user) {
            if (g_arch_copy_from_user(&frame, (const void __user *)(unsigned long)fp, sizeof(frame)) != 0) {
                break;
            }
        } else {
            break;
        }

        // 5.3 记录返回地址
        if (frame.ret_addr > 0x1000) {
            if (trace->nr_entries > 0 && trace->entries[trace->nr_entries - 1] != frame.ret_addr) {
                trace->entries[trace->nr_entries++] = frame.ret_addr;
            }
        }

        // 5.4 移动指针
        if (frame.next_fp <= fp) {
            break;
        }

        fp = frame.next_fp;
        depth++;
    }

    if (trace->nr_entries < trace->max_entries) {
        trace->entries[trace->nr_entries++] = (unsigned long)-1;
    }
}

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
        get_vma_info_str(ip, vma_info_buf, sizeof(vma_info_buf));
        
        pr_info("#%02d PC: %016lx%s\n", i, ip, vma_info_buf);
    }
    
    pr_info("------------------------------------------\n");
}

int stack_unwind_init(void)
{
    g_save_stack_trace_user = (save_stack_trace_user_t)kallsyms_lookup_name("save_stack_trace_user");
    if (!g_save_stack_trace_user) {
        pr_err("save_stack_trace_user missing\n");
        return -1;
    }

    g_arch_copy_from_user = (arch_copy_from_user_t)kallsyms_lookup_name("__arch_copy_from_user");
    if (!g_arch_copy_from_user) {
        pr_err("__arch_copy_from_user missing\n");
        return -1;
    }

    // VMA 相关符号查找
    g_find_vma = (find_vma_t)kallsyms_lookup_name("find_vma");
    g_file_path = (file_path_t)kallsyms_lookup_name("file_path");
    if (!g_file_path) {
        g_file_path = (file_path_t)kallsyms_lookup_name("d_path");
    }

    g_down_read_trylock = (down_read_trylock_t)kallsyms_lookup_name("down_read_trylock");
    g_up_read = (up_read_t)kallsyms_lookup_name("up_read");
    g_get_free_page = (get_free_page_t)kallsyms_lookup_name("__get_free_pages");
    g_free_page = (free_page_t)kallsyms_lookup_name("free_pages");
    g_snprintf = (snprintf_t)kallsyms_lookup_name("snprintf");

    if (!g_find_vma || !g_file_path || !g_down_read_trylock || !g_snprintf) {
        pr_warn("VMA/snprintf symbols missing, logging functionality restricted.\n");
    }

    return 0;
}
