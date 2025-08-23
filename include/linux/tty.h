#ifndef _TTY_H
#define _TTY_H

#include <termios.h>
// 定义 TTY 缓冲区大小为 1024 字节，用于存储终端输入 / 输出数据。
#define TTY_BUF_SIZE 1024
/**
 * data descp: 环形缓冲区
 */
// data：未实际使用（历史遗留或预留），早期可能用于标记数据状态。
// head：缓冲区 “写” 位置指针，指示下一个写入数据的索引。
// tail：缓冲区 “读” 位置指针，指示下一个读取数据的索引。
// proc_list：等待该缓冲区数据的进程链表（实现阻塞读：无数据时进程睡眠，有数据时唤醒）。
// buf[TTY_BUF_SIZE]：实际存储数据的环形缓冲区，大小由 TTY_BUF_SIZE 定义。
struct tty_queue
{
    unsigned long data;
    unsigned long head;
    unsigned long tail;
    struct task_struct *proc_list;
    char buf[TTY_BUF_SIZE];
};

// 这部分是核心！通过宏实现环形缓冲区的读写、判空 / 满等操作，
// 利用位运算 & (TTY_BUF_SIZE-1) 实现环形特性
// 假设 TTY_BUF_SIZE 是 2 的幂，TTY_BUF_SIZE-1 是掩码，等价取模）。

// 功能：缓冲区指针 a 递增，到末尾时回到开头（环形特性）。
#define INC(a) ((a) = ((a) + 1) & (TTY_BUF_SIZE - 1)) /*&的这个特性很牛逼，双重判定！*/
// 功能：指针 a 递减，同样通过掩码实现环形回绕（极少用，主要是对称 INC）。
#define DEC(a) ((a) = ((a) - 1) & (TTY_BUF_SIZE - 1))
// 功能：判断环形缓冲区是否为空（读写指针重合）。
#define EMPTY(a) ((a).head == (a).tail)
// 功能：计算缓冲区剩余空间（或数据长度），通过 head - tail 再掩码，避免负数问题。
#define LEFT(a) (((a).tail - (a).head - 1) & (TTY_BUF_SIZE - 1))
// 功能：获取缓冲区最后一个字节（用于特殊处理，如回退字符）。
#define LAST(a) ((a).buf[(TTY_BUF_SIZE - 1) & ((a).head - 1)])
// 功能：判断缓冲区是否满（剩余空间为 0）。
#define FULL(a) (!LEFT(a))
// 功能：计算缓冲区中有效数据长度（等价 LEFT(a) 的另一种写法，结果一致）。
#define CHARS(a) (((a).head - (a).tail) & (TTY_BUF_SIZE - 1))
// 功能：从缓冲区 queue 读一个字符到 c，并移动 tail 指针（实现 “读操作”）。
// 语法：GNU 扩展的语句表达式，把宏当函数用，避免多次求值问题。
#define GETCH(queue, c) \
    (void)({c=(queue).buf[(queue).tail];INC((queue).tail); })
// 功能：把字符 c 写入缓冲区 queue 的 head 位置，移动 head 指针（实现 “写操作”）。
#define PUTCH(c, queue) \
    (void)({(queue).buf[(queue).head]=(c);INC((queue).head); })

/**
 * func descp: 从 termios 结构体中提取控制字符（如中断、退出、删除等），
 * 方便统一管理终端行为。
 */
// 作用：通过宏快速获取终端配置的控制字符（如 VINTR 对应 Ctrl+C 中断），让代码更简洁。

// 定义获取终端各种控制字符的宏，简化对termios结构体中控制字符的访问
// 每个宏对应终端的特定控制功能，通过宏封装复杂的结构体成员访问，提升代码可读性

// 获取终端的中断控制字符（默认Ctrl+C）
// 功能：触发前台进程收到SIGINT信号，通常用于终止程序运行
#define INTR_CHAR(tty) ((tty)->termios.c_cc[VINTR])
// 获取终端的退出控制字符（默认Ctrl+\）
// 功能：触发前台进程收到SIGQUIT信号，终止程序并生成core dump调试文件
#define QUIT_CHAR(tty) ((tty)->termios.c_cc[VQUIT])
// 获取终端的删除字符（默认Backspace键）
// 功能：删除输入的上一个字符（如输入"abc"后按此键变为"ab"）
#define ERASE_CHAR(tty) ((tty)->termios.c_cc[VERASE])
// 获取终端的整行删除字符（默认Ctrl+U）
// 功能：删除当前输入行的所有字符（快速清空当前输入）
#define KILL_CHAR(tty) ((tty)->termios.c_cc[VKILL])
// 获取终端的文件结束字符（默认Ctrl+D）
// 功能：标记输入结束，在shell中无输入时按此键会退出shell
#define EOF_CHAR(tty) ((tty)->termios.c_cc[VEOF])
// 获取终端的输出恢复字符（默认Ctrl+Q）
// 功能：恢复被STOP_CHAR暂停的终端输出（与Ctrl+S配合使用）
#define START_CHAR(tty) ((tty)->termios.c_cc[VSTART])
// 获取终端的输出暂停字符（默认Ctrl+S）
// 功能：暂停终端输出（如防止大量数据刷屏，按Ctrl+Q恢复）
#define STOP_CHAR(tty) ((tty)->termios.c_cc[VSTOP])
// 获取终端的进程挂起字符（默认Ctrl+Z）
// 功能：触发前台进程收到SIGTSTP信号，使进程暂停并进入后台
#define SUSPEND_CHAR(tty) ((tty)->termios.c_cc[VSUSP])

// tty_struct 结构体：终端核心配置
struct tty_struct
{
    struct termios termios; /*终端属性（波特率、控制字符、回显等配置）。*/
    int pgrp;               /*终端所属进程组 ID，用于信号转发（如 Ctrl+C 发给前台进程组）。*/
    int stopped;            /*标记终端是否被暂停（如 Ctrl+Z 触发）。*/
                            /*函数指针write，指向实际的写操作函数（如串口写 rs_write、控制台写 con_write），实现多终端类型统一接口。*/
    void (*write)(struct tty_struct *tty);
    struct tty_queue read_q;    /*输入缓冲区（存储用户输入字符）。*/
    struct tty_queue write_q;   /*输出缓冲区（存储待输出到终端的字符）。*/
    struct tty_queue secondary; /*辅助缓冲区（处理 “加工后” 的输入，如回显、特殊字符过滤）。*/
};

/*声明全局数组 tty_table，存储系统中所有 TTY 设备的配置（多终端时用数组管理）。*/
extern struct tty_struct tty_table[];

/*定义终端控制字符的默认值（用转义字符表示 ^C/^Z 等），初始化 termios.c_cc 时使用。*/
// 具体如下：
/*	intr=^C		quit=^|		erase=del	kill=^U
    eof=^D		vtime=\0	vmin=\1		sxtc=\0
    start=^Q	stop=^S		susp=^Z		eol=\0
    reprint=^R	discard=^U	werase=^W	lnext=^V
    eol2=\0
*/
#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0"

// 初始化串口终端（Serial Port TTY）。
void rs_init(void);
// 初始化控制台终端（Console TTY）。
void con_init(void);
// 统一初始化 TTY 子系统。
void tty_init(void);
// 从 TTY c 读 n 字节到 buf。
int tty_read(unsigned c, char *buf, int n);
// 向 TTY c 写 buf 中 n 字节。
int tty_write(unsigned c, char *buf, int n);
// 串口 TTY 的写函数（发送数据到串口硬件）。
void rs_write(struct tty_struct *tty);
// 控制台 TTY 的写函数（输出到屏幕）。
void con_write(struct tty_struct *tty);
// 输入处理核心函数：将 read_q 中的 “原始输入”
// 加工到 secondary 缓冲区（处理回显、特殊字符过滤、换行转换等）。
void copy_to_cooked(struct tty_struct *tty);

#endif