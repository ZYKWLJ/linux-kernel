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
    /* these are hardcoded - don't touch--提示是核心，不可修改（与内核调度、内存布局强绑定） */
    // 1. 进程状态：标记进程当前的运行状态，决定调度器是否可选择该进程
    long state;
    /*
     * 取值定义：
     * -1: 不可运行态（TASK_UNINTERRUPTIBLE，如等待硬件资源，不响应信号）；
     *  0: 可运行态（TASK_RUNNING，在就绪队列或正在运行，调度器可调度）；
     * >0: 停止态（如 TASK_STOPPED 被信号暂停，或 TASK_ZOMBIE 僵尸态，等待父进程回收）；
     * 核心作用：调度器通过该字段筛选“可调度进程”，状态变更会触发调度逻辑（如进程从阻塞→就绪）。
     */
    

    // 2. 时间片计数器：实现时间片轮转调度的核心字段
    long counter;
    /*
     * 核心逻辑：
     * - 初始值 = 进程 priority（优先级），优先级越高，初始时间片越长；
     * - 每次时钟中断（10ms）触发递减，当 counter 减至 0 时，进程被标记为“需要重新调度”；
     * - 调度器选中该进程时，会重新将 counter 重置为 priority，实现“优先级+时间片”结合的调度策略；
     * 作用：防止单个进程独占 CPU，保证多进程公平性。
     */

    // 3. 进程静态优先级：决定时间片长度和调度权重的基础值
    long priority;
    /*
     * 设计规则：
     * - 数值越小，优先级越高（早期 Linux 优先级模型，与后期“数值越大优先级越高”相反）；
     * - 核心作用：
     *   1. 初始化 counter（时间片长度）：priority 直接决定进程每次获得的时间片时长；
     *   2. 调度器排序依据：就绪队列中，优先级高的进程会被优先选择运行；
     * - 注意：该值是“静态优先级”，运行中不会被内核自动修改（需通过系统调用手动调整）。
     */

    // 4. 未处理信号位图：标记进程收到但尚未处理的信号（如 SIGINT 中断信号、SIGKILL 终止信号）
    long signal;
    /*
     * 存储形式：32 位整数的位图（每一位对应一个信号，如 bit0 对应信号 1，bit1 对应信号 2）；
     * 核心作用：内核在进程调度/返回用户态前，会检查该字段，若有未处理信号则触发信号处理逻辑；
     * 关联：与下方 sigaction、blocked 配合，构成完整的信号处理机制。
     */

    // 5. 信号处理动作数组：定义每个信号的“处理方式”（POSIX 标准信号机制的核心结构）
    struct sigaction sigaction[32];
    /*
     * 数组规模：32 个元素，对应 Linux 0.11 支持的 32 种信号（信号 1~32）；
     * struct sigaction 核心字段（隐含）：
     *   - sa_handler：信号处理函数指针（可指向自定义函数、SIG_IGN 忽略、SIG_DFL 默认动作）；
     *   - sa_mask：处理该信号时的“临时阻塞信号掩码”（防止处理中被同类型信号打断）；
     *   - sa_flags：信号处理标志（如 SA_ONESHOT 处理后恢复默认动作）；
     * 作用：进程通过 sigaction() 系统调用修改该数组，自定义信号的响应行为。
     */

    // 6. 阻塞信号掩码：标记当前“被阻塞”的信号（被阻塞的信号不会触发处理，暂存于 signal 中）
    long blocked;
    /*
     * 存储形式：与 signal 一致的 32 位位图（某一位为 1，表示对应信号被阻塞）；
     * 核心逻辑：
     * - 内核检查信号时，会先对比 blocked 位图，若信号被阻塞，则不触发处理，仅保留在 signal 中；
     * - 进程可通过 sigprocmask() 系统调用修改该掩码，动态开启/关闭信号阻塞；
     * 作用：允许进程在关键操作（如数据写入）期间，暂时屏蔽特定信号，避免操作被打断。
     */

    /* various fields（进程运行相关的通用字段） */
    // 7. 进程退出码：存储进程的终止状态，供父进程回收
    int exit_code;
    /*
     * 核心场景：
     * - 进程正常退出（如 exit() 系统调用）时，设置为指定的退出码（0 表示成功，非 0 表示错误）；
     * - 进程异常终止（如信号杀死）时，设置为“信号编号 + 特殊标记”（如 128 + 信号号）；
     * 作用：父进程通过 wait()/waitpid() 系统调用读取该值，判断子进程的终止原因。
     */

    // 8. 进程内存区域边界：标记进程在内存中的代码段、数据段、栈段等关键地址，用于内存管理
    unsigned long start_code;  /* 代码段起始地址：进程可执行文件加载到内存的起始位置（只读）； */
    unsigned long end_code;    /* 代码段结束地址：代码段的末尾，后续是数据段； */
    unsigned long end_data;    /* 数据段结束地址：初始化数据段（.data）+ 未初始化数据段（.bss）的末尾； */
    unsigned long brk;         /* 堆段当前末尾地址：进程通过 brk()/sbrk() 系统调用扩展堆内存时，修改该值； */
    unsigned long start_stack; /* 用户栈起始地址：用户态栈的最高地址（栈从高地址向低地址增长）； */
                               /* 作用：内核通过这些地址划分进程内存区域，实现“地址空间隔离”和“内存保护”（如代码段只读）。 */

    // 9. 进程ID与亲属关系：标识进程唯一性及进程间的父子/组关系，用于进程管理
    long pid;     /* 进程唯一ID（Process ID）：内核全局唯一的标识符，用于通过 kill()/wait() 等操作指定进程； */
    long father;  /* 父进程PID（Parent PID）：指向创建当前进程的父进程，用于亲属关系遍历（如父进程回收子进程）； */
    long pgrp;    /* 进程组ID（Process Group ID）：多个进程组成一个组，用于“组信号”（如向整个进程组发送 SIGINT）； */
    long session; /* 会话ID（Session ID）：多个进程组组成一个会话，对应一个终端（如终端关闭时，会话内进程收到 SIGHUP）； */
    long leader;  /* 会话领导进程标记（1 表示是会话 leader，0 不是）：会话 leader 负责关联终端，终端退出时会通知该进程； */

    // 10. 用户身份标识：控制进程的文件访问权限、系统资源访问权限（Unix 安全模型核心）
    unsigned short uid;  /* 真实用户ID（Real UID）：进程创建者的用户ID，标识“谁实际拥有该进程”，不可随意修改； */
    unsigned short euid; /* 有效用户ID（Effective UID）：决定进程当前的权限（如是否能读写 root 权限文件），可通过 setuid() 修改； */
    unsigned short suid; /* 保存的用户ID（Saved UID）：用于临时切换权限后恢复（如 su 命令切换用户时，保存原 UID）； */
    unsigned short gid;  /* 真实组ID（Real GID）：进程创建者所属组的ID，对应文件权限中的“组权限”； */
    unsigned short egid; /* 有效组ID（Effective GID）：决定进程当前的组权限，类似 euid； */
    unsigned short sgid; /* 保存的组ID（Saved GID）：类似 suid，用于临时切换组权限后恢复； */

    // 11. 闹钟定时器：记录进程设置的闹钟时间，用于实现 alarm() 系统调用
    long alarm;
    /*
     * 存储形式：距离闹钟触发还剩的“时钟中断次数”（每个时钟中断 10ms，如 alarm=100 表示 1s 后触发）；
     * 核心逻辑：每次时钟中断递减，减至 0 时，向进程发送 SIGALRM 信号；
     * 作用：实现进程级的定时功能（如 sleep() 底层依赖类似逻辑）。
     */

    // 12. 进程时间统计：记录进程的 CPU 使用时间，用于资源统计和调度优化
    long utime;      /* 用户态 CPU 时间：进程在用户态运行的总时间（单位：时钟中断次数，10ms/次）； */
    long stime;      /* 内核态 CPU 时间：进程在内核态运行的总时间（如执行系统调用的时间）； */
    long cutime;     /* 子进程用户态总时间：当前进程所有已终止子进程的 utime 之和，供父进程统计； */
    long cstime;     /* 子进程内核态总时间：当前进程所有已终止子进程的 stime 之和； */
    long start_time; /* 进程创建时间：从系统启动到进程创建的时间（单位：时钟中断次数），用于计算进程存活时间； */

    // 13. 数学协处理器使用标记：标记进程是否使用 80387 数学协处理器（早期 x86 架构专用）
    unsigned short used_math;
    /*
     * 取值：0 未使用，1 已使用；
     * 核心逻辑：
     * - x86 架构中，数学协处理器状态需在进程切换时保存/恢复；
     * - 若进程未使用数学协处理器（used_math=0），调度时可跳过协处理器状态切换，提升性能；
     */

    /* file system info（文件系统相关字段：管理进程的文件访问上下文） */
    // 14. 进程关联的终端设备号：标记进程对应的终端（如控制台、串口）
    int tty;
    /*
     * 取值：-1 表示无关联终端（如后台守护进程），非负整数表示终端设备号（对应 /dev/ttyX 设备）；
     * 作用：进程的标准输入（stdin）、标准输出（stdout）默认指向该终端，终端操作（如键盘输入）会触发进程信号。
     */

    // 15. 文件创建掩码：控制新创建文件的默认权限（屏蔽指定的权限位）
    unsigned short umask;
    /*
     * 存储形式：8 位权限掩码（如 022 表示屏蔽“组写”和“其他写”权限）；
     * 核心逻辑：新文件的初始权限 = 进程指定的权限（如 0666） & (~umask)；
     * 作用：防止进程意外创建权限过宽的文件（如默认屏蔽“其他写”权限，保证安全性）。
     */

    // 16. 进程当前工作目录的 inode 指针：指向当前工作目录的索引节点（inode）
    struct m_inode *pwd;
    /*
     * 作用：进程使用相对路径（如 "./file.txt"）访问文件时，内核会以 pwd 指向的目录为起点解析路径；
     * 关联：cd 命令本质是修改该指针，指向新的工作目录 inode。
     */

    // 17. 进程根目录的 inode 指针：指向进程“根目录”的索引节点（默认是系统根目录 /）
    struct m_inode *root;
    /*
     * 特殊场景：chroot() 系统调用可修改该指针，将进程的根目录切换到指定目录（如沙箱隔离）；
     * 作用：进程使用绝对路径（如 "/file.txt"）访问文件时，以内核会以 root 指向的目录为起点解析。
     */

    // 18. 进程可执行文件的 inode 指针：指向当前进程运行的可执行文件的索引节点
    struct m_inode *executable;
    /*
     * 作用：
     * 1. 加载可执行文件时，内核通过该指针读取文件的代码段、数据段信息；
     * 2. 进程异常崩溃时，可通过该指针定位崩溃的可执行文件；
     */

    // 19. 执行 exec() 时需关闭的文件掩码：标记进程中“执行 exec() 系统调用后需自动关闭”的文件
    unsigned long close_on_exec;
    /*
     * 存储形式：32 位位图（每一位对应一个文件描述符，如 bit0 对应 fd=0）；
     * 核心逻辑：进程调用 exec() 加载新程序时，若文件描述符的对应位为 1，则自动关闭该文件；
     * 作用：防止新程序意外继承原程序的不必要文件句柄（如网络连接、临时文件）。
     */

    // 20. 文件描述符表：存储进程打开的文件的指针数组，是进程访问文件的核心接口
    struct file *filp[NR_OPEN];
    /*
     * NR_OPEN：宏定义，表示进程最大可打开的文件数（Linux 0.11 中默认是 20）；
     * 核心逻辑：
     * - 进程通过 open() 打开文件时，内核分配一个 struct file 结构体，将指针存入该数组，返回数组下标（即文件描述符 fd）；
     * - 后续 read()/write()/close() 等操作，通过 fd 索引该数组，找到对应的文件结构体；
     * 作用：管理进程的所有打开文件，实现“文件描述符→文件”的映射。
     */

    /* ldt for this task 0 - zero 1 - cs 2 - ds&ss（进程的局部描述符表，x86 架构内存保护核心） */
    struct desc_struct ldt[3];
    /*
     * 背景：x86 架构中，LDT（局部描述符表）用于定义进程专属的内存段（与全局 GDT 区分）；
     * 数组结构（3 个描述符，固定用途，不可修改）：
     * - ldt[0]：空描述符（x86 要求 LDT 第一个描述符必须为 0，用于容错）；
     * - ldt[1]：代码段描述符（CS 段寄存器指向该描述符，定义进程代码段的基地址、限长、权限）；
     * - ldt[2]：数据段/栈段描述符（DS、SS 段寄存器指向该描述符，定义数据段/栈段的内存属性）；
     * 作用：通过 LDT 实现“进程地址空间隔离”——每个进程的 LDT 不同，确保进程只能访问自己的内存段。
     */

    /* tss for this task（进程的任务状态段，x86 架构进程切换核心） */
    struct tss_struct tss;
    /*
     * 背景：x86 架构中，TSS（任务状态段）是一个特殊内存区域，存储进程的硬件上下文（寄存器状态）；
     * struct tss_struct 核心字段（隐含）：
     * - 通用寄存器（ax、bx、cx、dx 等）、段寄存器（cs、ds 等）、指令指针（eip）、栈指针（esp）；
     * - 页目录基址寄存器（CR3，用于页表切换）、数学协处理器状态；
     * 核心逻辑：
     * - 进程切换时，内核将当前进程的寄存器状态保存到其 TSS 中；
     * - 加载新进程时，从其 TSS 中恢复寄存器状态，实现“上下文切换”；
     * 作用：TSS 是 x86 硬件支持进程切换的关键结构，内核通过修改 TSS 完成进程上下文的保存与恢复。
     */
};

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x9ffff (=640kB)
 */

// 这段代码是Linux 0.11内核中定义初始任务(INIT_TASK)的宏，
// 用于设置第一个任务表(即0号进程，swapper进程)的数据结构。
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

// 作用：声明一个全局数组task，用于存储系统中所有进程的控制块指针。
// 细节：NR_TASKS定义了系统最大进程数（如 64），数组下标对应进程 ID（PID），
// 每个元素指向对应进程的task_struct（进程控制块），NULL表示该 PID 无进程。
extern struct task_struct *task[NR_TASKS];

// 作用：声明一个指针，记录最后使用过数学协处理器（80387）的进程。
// 细节：x86 架构中，协处理器状态需在进程切换时保存 / 恢复，通过该指针
// 可优化切换效率（仅恢复上一个使用协处理器的进程状态）。

extern struct task_struct *last_task_used_math;

// 作用：声明一个指针，指向当前正在 CPU 上运行的进程的控制块。
extern struct task_struct *current;

// 作用：声明一个全局变量，记录系统启动后的时钟中断总次数。
extern long volatile jiffies;

// 作用：声明一个全局变量，存储系统启动时的 Unix 时间戳（
// 从 1970 年 1 月 1 日到启动时刻的秒数）。
extern long startup_time;

// 作用：定义一个宏，计算当前系统的 Unix 时间戳（秒级）。

#define CURRENT_TIME (startup_time + jiffies / HZ)

// 作用：注册一个定时器，在指定的jiffies时刻执行回调函数fn。
extern void add_timer(long jiffies, void (*fn)(void));

// 作用：将当前进程置为 “不可中断睡眠态”，并加入p指向的等待队列，等待资源就绪。
extern void sleep_on(struct task_struct **p);

// 作用：将当前进程置为 “不可中断睡眠态”，并加入p指向的等待队列，等待资源就绪。
extern void interruptible_sleep_on(struct task_struct **p);

// 作用：唤醒p指向的等待队列中**所有睡眠的进程。**
extern void wake_up(struct task_struct **p);

// Linux 0.11 内核中与全局描述符表（GDT）、任务状态段（TSS）和
// 局部描述符表（LDT）相关的宏定义，用于 x86 架构下的内存分段和任务切换。

/*
 * Entry into gdt where to find first TSS. 0-nul, 1-cs, 2-ds, 3-syscall
 * 4-TSS0, 5-LDT0, 6-TSS1 etc ...
 */
// 这段英文说明了GDT表中数据结构的存放：
// 说明：解释 GDT（全局描述符表）中各个表项的布局：
// 0：空描述符（x86 架构要求 GDT 第一个表项必须为 0，用于容错）；
// 1：代码段（CS）描述符；
// 2：数据段（DS）描述符；
// 3：系统调用相关描述符；
// 4：第一个任务的 TSS（任务状态段）描述符（TSS0）；
// 5：第一个任务的 LDT（局部描述符表）描述符（LDT0）；
// 6：第二个任务的 TSS 描述符（TSS1），以此类推（TSSn 和 LDTn 交替排列）。

// 作用：定义 GDT 中第一个 TSS 描述符的索引为 4（对应注释中的 “4-TSS0”）。
// 意义：后续所有任务的 TSS 描述符都从索引 4 开始排列，方便通过宏计算任意任务的 TSS 在 GDT 中的位置。
#define FIRST_TSS_ENTRY 4

// 作用：定义 GDT 中第一个 LDT 描述符的索引为 5（4+1）（对应注释中的 “5-LDT0”）。
// 逻辑：由于 TSS 和 LDT 描述符交替排列（TSS0→LDT0→TSS1→LDT1…），因此第一个 LDT 索引是第一个 TSS 索引加 1。
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY + 1)

// 作用：计算第n个任务的 TSS 描述符在 GDT 中的 “选择子”（Selector）值。
// 解析：
// x86 架构中，GDT 描述符选择子是 16 位值，结构为：[索引(13位) | TI位(1位) | RPL位(2位)]。

// FIRST_TSS_ENTRY << 3：
// 将第一个 TSS 的索引（4）左移 3 位（4<<3=32），相当于索引×8（因为每个 GDT 表项占 8 字节，左移 3 位等价于乘 8），
// 得到 TSS0 在 GDT 中的字节偏移量的基础值。

// ((unsigned long)n) << 4：
// n是任务编号（0 开始），左移 4 位等价于n×16。由于每个任务占用两个 GDT 表项（TSS 和 LDT 各占 8 字节，共 16 字节），
// 因此第n个任务的 TSS 相对于第一个 TSS 的偏移量是n×16。
// 总和即为第n个任务的 TSS 描述符在 GDT 中的选择子（高 13 位为索引，低 3 位中 TI=0 表示 GDT，RPL=0 表示最高权限）。
#define _TSS(n) ((((unsigned long)n) << 4) + (FIRST_TSS_ENTRY << 3))
// 作用：计算第n个任务的 LDT 描述符在 GDT 中的选择子值。

// 解析：逻辑与_TSS(n)类似：
// FIRST_LDT_ENTRY << 3：第一个 LDT 的索引（5）左移 3 位（5<<3=40），得到 LDT0 在 GDT 中的字节偏移量基础值。
// n << 4：同样表示第n个任务的 LDT 相对于第一个 LDT 的偏移量（n×16字节）。
// 总和即为第n个任务的 LDT 描述符选择子。
#define _LDT(n) ((((unsigned long)n) << 4) + (FIRST_LDT_ENTRY << 3))

// 作用：定义宏ltr(n)，用于加载第n个任务的 TSS 选择子到 TR（任务寄存器）。

// 解析：
// __asm__内嵌汇编：执行ltr %%ax指令（Load Task Register），将ax寄存器中的值加载到 TR。
// :"a"(_TSS(n))：将第n个任务的 TSS 选择子（_TSS(n)的结果）传入ax寄存器。
// 意义：TR 寄存器用于指向当前任务的 TSS 描述符，x86 硬件通过 TR 找到 TSS，从而实现任务切换时的上下文保存 / 恢复。
#define ltr(n) __asm__("ltr %%ax" ::"a"(_TSS(n)))

// 作用：定义宏lldt(n)，用于加载第n个任务的 LDT 选择子到 LDTR（局部描述符表寄存器）。
// 解析：
// 内嵌汇编执行lldt %%ax指令（Load LDT Register），将ax中的 LDT 选择子（_LDT(n)的结果）加载到 LDTR。
// 意义：LDTR 指向当前任务的 LDT 描述符，内核通过 LDTR 找到进程的局部描述符表，实现进程地址空间的隔离。
#define lldt(n) __asm__("lldt %%ax" ::"a"(_LDT(n)))

// 作用：定义宏str(n)，用于获取当前正在运行的任务编号（n为输出参数）。
// 解析：
// 内嵌汇编步骤：
// str %%ax：执行str指令（Store Task Register），将 TR 寄存器中的 TSS 选择子存入ax。
// subl %2,%%eax：%2是输入参数FIRST_TSS_ENTRY << 3（即 32），用ax减去 32，得到相对于第一个 TSS 的偏移量。
// shrl $4,%%eax：将结果右移 4 位（等价于除以 16），得到任务编号n（因为每个任务占 16 字节的 GDT 空间）。
// 输出："=a"(n)表示最终结果存入变量n，即当前运行的任务编号。
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

//  switch_to(n)宏的功能：

// 用于将任务切换到编号为n的进程；
// 首先检查n是否为当前任务，若是则不执行任何操作；
// 若切换到的任务是最后使用过数学协处理器的进程，
// 需清除 CR0 寄存器中的 TS 标志位（Task Switched，用于协处理器状态管理）。

// 作用：实现进程切换的核心宏，将 CPU 执行权从当前进程切换到编号为n的进程。
// 逐行解析：
// struct { long a, b; } __tmp;：定义临时结构体，用于内嵌汇编中的跳转地址存储。
// 内嵌汇编指令详解：
// cmpl %%ecx,current\n\t：比较ecx（存放task[n]指针）与current（当前进程指针）。
// je 1f\n\t：若相等（即切换到自身），跳转到标签1:（不执行切换）。
// movw %%dx,%1\n\t：将dx寄存器中的 TSS 选择子（_TSS(n)的结果）存入__tmp.b。
// xchgl %%ecx,current\n\t：交换ecx和current的值，使current指向新进程（task[n]）。
// ljmp *%0\n\t：执行远跳转，通过__tmp.a间接寻址，实际是跳转到新进程的 TSS，触发硬件任务切换（x86 硬件会自动保存当前上下文到旧 TSS，加载新 TSS 中的上下文）。
// cmpl %%ecx,last_task_used_math\n\t：切换后，比较新进程（ecx）是否为最后使用协处理器的进程。
// jne 1f\n\t：若不是，跳转到1:。
// clts\n：若是，则执行clts指令清除 CR0 寄存器的 TS 标志（允许新进程使用协处理器，无需再触发异常保存旧状态）。
// 1:：跳转标签，切换结束。
// 输入参数：
// m(*&__tmp.a)/m(*&__tmp.b)：临时变量的内存地址，用于跳转和存储 TSS 选择子。
// d(_TSS(n))：dx寄存器存放第n个任务的 TSS 选择子。
// c((long)task[n])：ecx寄存器存放第n个任务的task_struct指针。

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

// 作用：将地址n向上对齐到最近的页边界（4KB 对齐，x86 架构默认页大小为 4096 字节 = 0x1000）。

// 解析：
// (n) + 0xfff：将地址加上 4095，确保超过当前页边界。
// & 0xfffff000：与操作清除低 12 位（页内偏移），得到对齐后的页起始地址。
// 用途：内核内存分配时确保地址按页对齐，适配硬件分页机制要求。

#define PAGE_ALIGN(n) (((n) + 0xfff) & 0xfffff000)

// 作用：设置内存段描述符（如 LDT、GDT 中的段描述符）的基地址（base）。

// 背景：x86 段描述符是 8 字节结构，基地址占 32 位，分散存储在描述符的第 2-4 字节和第 7 字节（低 16 位在 2-3 字节，中 8 位在 4 字节，高 8 位在 7 字节）。
// 解析：

// 内嵌汇编步骤：
// push %%edx：保存edx寄存器。
// movw %%dx,%0：将base的低 16 位（dx）写入描述符的 2-3 字节（addr+2）。
// rorl $16,%%edx：将edx循环右移 16 位，使base的高 16 位移到低 16 位。
// movb %%dl,%1：将base的中 8 位（dl）写入描述符的第 4 字节（addr+4）。
// movb %%dh,%2：将base的高 8 位（dh）写入描述符的第 7 字节（addr+7）。
// pop %%edx：恢复edx。

// 参数：addr是段描述符地址，base是 32 位基地址值。

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

// 作用：设置内存段描述符的限长（limit），即段的最大可访问范围。

// 背景：x86 段描述符的限长占 20 位，存储在描述符的 0-1 字节（低 16 位）和第 6 字节的低 4 位（高 4 位）。

// 解析：
// 内嵌汇编步骤：
// push %%edx：保存edx。
// movw %%dx,%0：将limit的低 16 位（dx）写入描述符的 0-1 字节（addr）。
// rorl $16,%%edx：edx循环右移 16 位，使limit的高 16 位移到低 16 位。
// movb %1,%%dh：读取描述符第 6 字节（addr+6）到dh。
// andb $0xf0,%%dh：清除dh的低 4 位（保留高 4 位的段属性）。
// orb %%dh,%%dl：将limit的高 4 位（dl的低 4 位）与dh的高 4 位属性合并。
// movb %%dl,%1：将合并结果写回描述符第 6 字节（addr+6）。
// pop %%edx：恢复edx。

// 参数：addr是段描述符地址，limit是 20 位限长值。

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

// 作用：封装_set_base和_set_limit，提供更易用的接口（直接传入段描述符结构体ldt，而非地址）。

// 解析：

// set_base：将ldt结构体的地址转换为char*，传给_set_base设置基地址。
// set_limit：将limit（字节数）转换为页粒度的限长（(limit-1) >> 12），因为段描述符的限长在页粒度下以 4KB 为单位。

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
// 作用：从段描述符中读取 32 位基地址（与_set_base对应）。

// 解析：

// 内嵌汇编步骤：
// movb %3,%%dh：将描述符第 7 字节（高 8 位基地址）存入dh。
// movb %2,%%dl：将描述符第 4 字节（中 8 位基地址）存入dl。
// shll $16,%%edx：edx左移 16 位，将高 16 位（dh和dl）移到高位。
// movw %1,%%dx：将描述符 2-3 字节（低 16 位基地址）存入dx，拼接成完整 32 位基地址。
// 输出："=&d"(__base)表示结果存入__base并返回。

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

// 作用：封装_get_base，允许直接传入段描述符结构体ldt（而非地址），获取其基地址。

#define get_base(ldt) _get_base(((char *)&(ldt)))

// 作用：根据段选择子（segment）获取对应段的限长（字节数）。

// 解析：

// lsll %1,%0：执行lsl指令（Load Segment Limit），从段选择子segment指向的描述符中读取限长到__limit。
// incl %0：将限长加 1（因为描述符中存储的是 “最大偏移值”，实际长度为偏移值 + 1）。
// 用途：快速获取某个段（如代码段、数据段）的长度，用于内存越界检查。

#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__("lsll %1,%0\n\tincl %0":"=r" (__limit):"r" (segment)); \
__limit; })

#endif