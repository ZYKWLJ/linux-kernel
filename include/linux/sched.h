#ifndef _SCHED_H
#define _SCHED_H

#define NR_TASKS 64 /*系统支持的最大进程数*/
#define HZ 100      /*时钟中断频率，100次/秒*/

#define FIRST_TASK task[0]           /*定义第一个进程的宏*/
#define LAST_TASK task[NR_TASKS - 1] /*定义最后一个进程的宏*/

#include <linux/head.h>/*内核头部文件*/
#include <linux/fs.h>  /*文件系统头文件*/
#include <linux/mm.h>  /*内存管理头文件 */
#include <signal.h>    /*信号处理头文件*/

// 编译时检查：如果打开文件数 NR_OPEN 大于 32，则报错。
// 因为当时的实现中，close-on-exec 标志用一个字（32 位）表示，最多只能处理 32 个文件。

#if (NR_OPEN > 32)
#error "Currently the close-on-exec-flags are in one word, max 32 files/proc"
#endif

// 任务状态的定义
// 任务运行态
#define TASK_RUNNING 0
// 任务可中断态
#define TASK_INTERRUPTIBLE 1
// 任务不可中断态
#define TASK_UNINTERRUPTIBLE 2
// 任务僵尸态
#define TASK_ZOMBIE 3
// 任务停止态
#define TASK_STOPPED 4

// 防止NULL未定义，万无一失
#ifndef NULL
#define NULL ((void *)0)
#endif

// 复制页表
extern int copy_page_tables(unsigned long from, unsigned long to, long size);
// 释放页表
extern int free_page_tables(unsigned long from, unsigned long size);

/**
 * func descp: 生命调度和系统初始化的相关函数
 */
// 调度系统初始化
extern void sched_init(void);
// 调度进程函数
extern void schedule(void);
// 陷阱(中断)处理初始化函数
extern void trap_init(void);

// 这是内核的panic函数，用于处理内核无法恢复的严重错误，当内核遇到无法解决的
// 致命问题时会调用该函数，进入 "内核恐慌"（Kernel Panic）状态。
#ifndef PANIC
void panic(const char *str);
#endif

// 终端写操作函数
extern int tty_write(unsigned minor, char *buf, int count);

// 函数指针定义(通过类型来维系的！)
// fn_ptr是一个无参数的返回类型为int的函数指针类型
typedef int (*fn_ptr)();

// 定义 i387 协处理器状态结构，用于保存浮点寄存器等信息。
struct i387_struct
{
    long cwd;
    long swd;
    long twd;
    long fip;
    long fcs;
    long foo;
    long fos;
    long st_space[20]; /* 8*10 bytes for each FP-reg = 80 bytes */
};

// 定义任务状态段（TSS）结构，这是 x86 架构特有的结构，
// 用于保存`任务切换时`的`处理器状态`。
// 包含各种寄存器值、栈指针、页目录基址等信息。
struct tss_struct
{
    long back_link; /* 16 high bits zero */
    long esp0;
    long ss0; /* 16 high bits zero */
    long esp1;
    long ss1; /* 16 high bits zero */
    long esp2;
    long ss2; /* 16 high bits zero */
    long cr3;
    long eip;
    long eflags;
    long eax, ecx, edx, ebx;
    long esp;
    long ebp;
    long esi;
    long edi;
    long es;                 /* 16 high bits zero */
    long cs;                 /* 16 high bits zero */
    long ss;                 /* 16 high bits zero */
    long ds;                 /* 16 high bits zero */
    long fs;                 /* 16 high bits zero */
    long gs;                 /* 16 high bits zero */
    long ldt;                /* 16 high bits zero */
    long trace_bitmap;       /* bits: trace 0, bitmap 16-31 */
    struct i387_struct i387; /*协处理器的状态*/
};

/**
 * data descp: 定义任务（进程）控制块结构，包含进程管理所需的全部信息.
 * 这是重中之重了！敲重点！
 */

struct task_struct
{
    /* these are hardcoded - don't touch--提示是核心，不可修改 */
    // 任务状态
    long state; /* -1 unrunnable, 0 runnable, >0 stopped */
    // 进程时间片计数器，用于调度算法
    // - 初始值由 priority（优先级）决定，每次时钟中断递减
    // - 当 counter 减至 0 时，进程被抢占，重新调度
    // - 调度时会根据优先级重置该值，实现 “优先级调度 + 时间片轮转”
    long counter;
    // 进程优先级
    // 进程静态优先级，决定时间片的初始长度：
    // - 优先级数值越小，优先级越高（早期 Linux 优先级设计）
    // - 调度器会基于此值初始化 counter，优先级高的进程获得更长时间片
    long priority;
    // 信号处理字段
    long signal;
    //
    struct sigaction sigaction[32];
    //
    long blocked; /* bitmap of masked signals */
                  /* various fields */
    // 退出码
    int exit_code;

    unsigned long start_code, end_code, end_data, brk, start_stack;
    // 分别是：进程号、父进程号、进程组号、会话号、领导进程号
    long pid, father, pgrp, session, leader;
    //
    unsigned short uid, euid, suid;
    //
    unsigned short gid, egid, sgid;
    //
    long alarm;
    long utime, stime, cutime, cstime, start_time;
    unsigned short used_math;
    /* file system info */
    int tty; /* -1 if no tty, so it must be signed */
    unsigned short umask;
    struct m_inode *pwd;
    struct m_inode *root;
    struct m_inode *executable;
    unsigned long close_on_exec;
    struct file *filp[NR_OPEN];
    /* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
    struct desc_struct ldt[3];
    /* tss for this task */
    struct tss_struct tss;
};

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x9ffff (=640kB)
 */
#define INIT_TASK                                                                                                                                                              \
    /* state etc */ {                                                                                                                                                          \
        0,                                                                                                                                                                     \
        15,                                                                                                                                                                    \
        15,                                                                                                                                                                    \
        /* signals */ 0,                                                                                                                                                       \
        {                                                                                                                                                                      \
            {},                                                                                                                                                                \
        },                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        /* ec,brk... */ 0,                                                                                                                                                     \
        0,                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        /* pid etc.. */ 0,                                                                                                                                                     \
        -1,                                                                                                                                                                    \
        0,                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        /* uid etc */ 0,                                                                                                                                                       \
        0,                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        /* alarm */ 0,                                                                                                                                                         \
        0,                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        0,                                                                                                                                                                     \
        /* math */ 0,                                                                                                                                                          \
        /* fs info */ -1,                                                                                                                                                      \
        0022,                                                                                                                                                                  \
        NULL,                                                                                                                                                                  \
        NULL,                                                                                                                                                                  \
        NULL,                                                                                                                                                                  \
        0,                                                                                                                                                                     \
        /* filp */ {                                                                                                                                                           \
            NULL,                                                                                                                                                              \
        },                                                                                                                                                                     \
        {                                                                                                                                                                      \
            {0, 0},                                                                                                                                                            \
            /* ldt */ {0x9f, 0xc0fa00},                                                                                                                                        \
            {0x9f, 0xc0f200},                                                                                                                                                  \
        },                                                                                                                                                                     \
        /*tss*/ {0, PAGE_SIZE + (long)&init_task, 0x10, 0, 0, 0, 0, (long)&pg_dir, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, _LDT(0), 0x80000000, {}}, \
    }

extern struct task_struct *task[NR_TASKS];
extern struct task_struct *last_task_used_math;
extern struct task_struct *current;
extern long volatile jiffies;
extern long startup_time;

#define CURRENT_TIME (startup_time + jiffies / HZ)

extern void add_timer(long jiffies, void (*fn)(void));
extern void sleep_on(struct task_struct **p);
extern void interruptible_sleep_on(struct task_struct **p);
extern void wake_up(struct task_struct **p);

/*
 * Entry into gdt where to find first TSS. 0-nul, 1-cs, 2-ds, 3-syscall
 * 4-TSS0, 5-LDT0, 6-TSS1 etc ...
 */
#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY + 1)
#define _TSS(n) ((((unsigned long)n) << 4) + (FIRST_TSS_ENTRY << 3))
#define _LDT(n) ((((unsigned long)n) << 4) + (FIRST_LDT_ENTRY << 3))
#define ltr(n) __asm__("ltr %%ax" ::"a"(_TSS(n)))
#define lldt(n) __asm__("lldt %%ax" ::"a"(_LDT(n)))
#define str(n)                  \
    __asm__("str %%ax\n\t"      \
            "subl %2,%%eax\n\t" \
            "shrl $4,%%eax"     \
            : "=a"(n)           \
            : "a"(0), "i"(FIRST_TSS_ENTRY << 3))
/*
 *	switch_to(n) should switch tasks to task nr n, first
 * checking that n isn't the current task, in which case it does nothing.
 * This also clears the TS-flag if the task we switched to has used
 * tha math co-processor latest.
 */
#define switch_to(n)                                 \
    {                                                \
        struct                                       \
        {                                            \
            long a, b;                               \
        } __tmp;                                     \
        __asm__("cmpl %%ecx,current\n\t"             \
                "je 1f\n\t"                          \
                "movw %%dx,%1\n\t"                   \
                "xchgl %%ecx,current\n\t"            \
                "ljmp *%0\n\t"                       \
                "cmpl %%ecx,last_task_used_math\n\t" \
                "jne 1f\n\t"                         \
                "clts\n"                             \
                "1:" ::"m"(*&__tmp.a),               \
                "m"(*&__tmp.b),                      \
                "d"(_TSS(n)), "c"((long)task[n]));   \
    }

#define PAGE_ALIGN(n) (((n) + 0xfff) & 0xfffff000)

#define _set_base(addr, base)                 \
    __asm__("push %%edx\n\t"                  \
            "movw %%dx,%0\n\t"                \
            "rorl $16,%%edx\n\t"              \
            "movb %%dl,%1\n\t"                \
            "movb %%dh,%2\n\t"                \
            "pop %%edx" ::"m"(*((addr) + 2)), \
            "m"(*((addr) + 4)),               \
            "m"(*((addr) + 7)),               \
            "d"(base))

#define _set_limit(addr, limit)         \
    __asm__("push %%edx\n\t"            \
            "movw %%dx,%0\n\t"          \
            "rorl $16,%%edx\n\t"        \
            "movb %1,%%dh\n\t"          \
            "andb $0xf0,%%dh\n\t"       \
            "orb %%dh,%%dl\n\t"         \
            "movb %%dl,%1\n\t"          \
            "pop %%edx" ::"m"(*(addr)), \
            "m"(*((addr) + 6)),         \
            "d"(limit))

#define set_base(ldt, base) _set_base(((char *)&(ldt)), (base))
#define set_limit(ldt, limit) _set_limit(((char *)&(ldt)), (limit - 1) >> 12)

/**
#define _get_base(addr) ({\
unsigned long __base; \
__asm__("movb %3,%%dh\n\t" \
    "movb %2,%%dl\n\t" \
    "shll $16,%%edx\n\t" \
    "movw %1,%%dx" \
    :"=d" (__base) \
    :"m" (*((addr)+2)), \
     "m" (*((addr)+4)), \
     "m" (*((addr)+7)) \
        :"memory"); \
__base;})
**/

static inline unsigned long _get_base(char *addr)
{
    unsigned long __base;
    __asm__("movb %3,%%dh\n\t"
            "movb %2,%%dl\n\t"
            "shll $16,%%edx\n\t"
            "movw %1,%%dx"
            : "=&d"(__base)
            : "m"(*((addr) + 2)),
              "m"(*((addr) + 4)),
              "m"(*((addr) + 7)));
    return __base;
}

#define get_base(ldt) _get_base(((char *)&(ldt)))

#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__("lsll %1,%0\n\tincl %0":"=r" (__limit):"r" (segment)); \
__limit; })

#endif

struct task_struct
{
    /* these are hardcoded - don't touch */
    long state; /* -1 unrunnable, 0 runnable, >0 stopped */
    long counter;
    long priority;
    long signal;
    struct sigaction sigaction[32];
    long blocked; /* bitmap of masked signals */
                  /* various fields */
    int exit_code;
    unsigned long start_code, end_code, end_data, brk, start_stack;
    long pid, father, pgrp, session, leader;
    unsigned short uid, euid, suid;
    unsigned short gid, egid, sgid;
    long alarm;
    long utime, stime, cutime, cstime, start_time;
    unsigned short used_math;
    /* file system info */
    int tty; /* -1 if no tty, so it must be signed */
    unsigned short umask;
    struct m_inode *pwd;
    struct m_inode *root;
    struct m_inode *executable;
    unsigned long close_on_exec;
    struct file *filp[NR_OPEN];
    /* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
    struct desc_struct ldt[3];
    /* tss for this task */
    struct tss_struct tss;
};