#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h>

// 基础类型定义（符合 POSIX 信号处理规范）
typedef int sig_atomic_t;      // 原子信号类型（保证读写原子性）
typedef unsigned int sigset_t; // 信号集合类型（32位，对应常见32种信号) /* 32 bits */

#define _NSIG 32   // 系统支持的最大信号数（0~31，共32个）
#define NSIG _NSIG // 暴露给用户的信号数量宏

// 常用信号宏（POSIX 标准信号，1~31 为常规信号）
#define SIGHUP 1     // 挂起信号（终端断开）
#define SIGINT 2     // 中断信号（Ctrl+C）
#define SIGQUIT 3    // 退出信号（Ctrl+\，带 core dump）
#define SIGILL 4     // 非法指令
#define SIGTRAP 5    // 调试陷阱
#define SIGABRT 6    // 异常终止（abort() 触发）
#define SIGIOT 6     // 同 SIGABRT（历史兼容）
#define SIGUNUSED 7  // 未使用（保留）
#define SIGFPE 8     // 浮点异常
#define SIGKILL 9    // 强制杀死（无法捕获/忽略）
#define SIGUSR1 10   // 用户自定义信号1
#define SIGSEGV 11   // 段错误（非法内存访问）
#define SIGUSR2 12   // 用户自定义信号2
#define SIGPIPE 13   // 管道断裂（写已关闭的管道）
#define SIGALRM 14   // 定时器信号（alarm() 触发）
#define SIGTERM 15   // 终止信号（优雅退出，可捕获）
#define SIGSTKFLT 16 // 栈溢出（特定架构）
#define SIGCHLD 17   // 子进程状态变化（退出/停止）
#define SIGCONT 18   // 继续执行（之前被暂停）
#define SIGSTOP 19   // 暂停执行（无法捕获/忽略）
#define SIGTSTP 20   // 终端暂停（Ctrl+Z，可捕获）
#define SIGTTIN 21   // 后台进程读终端（非法操作）
#define SIGTTOU 22   // 后台进程写终端（非法操作）

/* Ok, I haven't implemented sigactions, but trying to keep headers POSIX */
// sigaction 标志宏（部分实现，贴近 POSIX 头文件规范）

#define SA_NOCLDSTOP 1        // 子进程停止时不触发 SIGCHLD
#define SA_NOMASK 0x40000000  // 旧版掩码标志（部分系统兼容）
#define SA_ONESHOT 0x80000000 // 信号处理函数仅生效一次

// sigprocmask 操作宏（控制信号掩码）
#define SIG_BLOCK 0   // 阻塞指定信号/* for blocking signals */
#define SIG_UNBLOCK 1 // 解除阻塞指定信号/* for unblocking signals */
#define SIG_SETMASK 2 // 直接设置信号掩码/* for setting the signal mask */
// 默认处理（系统默认逻辑）
#define SIG_DFL ((void (*)(int))0) /* default signal handling */
// 忽略信号（不做处理）
#define SIG_IGN ((void (*)(int))1) /* ignore signal */

// sigaction 结构体（POSIX 标准结构，用于高级信号处理）
struct sigaction
{
    void (*sa_handler)(int);   // 信号处理函数（或 SIG_DFL/SIG_IGN）
    sigset_t sa_mask;          // 信号掩码（处理信号时额外阻塞的信号）
    int sa_flags;              // 标志位（如 SA_NOCLDSTOP）
    void (*sa_restorer)(void); // 历史兼容字段（现代系统少用）
};

// 注册信号处理函数
// 这是最经典的信号声明了，之前专门写过一篇文章来分析这个声明
// signal是一个参数依次为int、返回void类型的，参数为int的函数指针的，
// 返回值为返回void类型的、参数为int的函数指针的函数。
void (*signal(int _sig, void (*_func)(int)))(int);

// 显然返回值类型为：去掉函数名和紧跟在其后的参数列表，即void (*)(int);，那显然是一个函数指针！
// void (*a())(int);
// int a();

// 信号处理核心函数声明（符合 POSIX 接口）
int raise(int sig);                       // 向自身发送信号
int kill(pid_t pid, int sig);             // 向指定进程/进程组发信号
int sigaddset(sigset_t *mask, int signo); // 向信号集合添加信号
int sigdelset(sigset_t *mask, int signo); // 从信号集合删除信号
int sigemptyset(sigset_t *mask);          // 清空信号集合
int sigfillset(sigset_t *mask);           // 填满信号集合（包含所有信号）
// 检查信号是否在集合中（1=是，0=否，-1=错） /* 1 - is, 0 - not, -1 error */
int sigismember(sigset_t *mask, int signo);
int sigpending(sigset_t *set);                                           // 获取当前pending（待处理）的信号集合
int sigprocmask(int how, sigset_t *set, sigset_t *oldset);               // 控制信号掩码
int sigsuspend(sigset_t *sigmask);                                       // 临时替换掩码并等待信号
int sigaction(int sig, struct sigaction *act, struct sigaction *oldact); // 高级信号处理

#endif /* _SIGNAL_H */