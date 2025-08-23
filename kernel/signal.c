#include <linux/sched.h>  // 包含进程调度相关定义（如current进程结构体）
#include <linux/kernel.h> // 包含内核基本函数和宏定义
#include <asm/segment.h>  // 包含段操作函数（如put_fs_byte、get_fs_byte等）
#include <signal.h>       // 包含信号相关常量和结构体定义（如sigaction、SIGKILL等）

void do_exit(int error_code); // 声明进程退出函数（在exit.c中实现）
// 作用：系统调用，获取当前进程的信号掩码（被阻塞的信号集合）。
// current：指向当前运行进程的结构体指针（在 sched.h 中定义）。
// blocked：进程结构体中记录被阻塞信号的字段（位掩码，每一位代表一个信号是否被阻塞）。
int sys_sgetmask()
{
    return current->blocked;
}

// 作用：系统调用，设置当前进程的信号掩码。
// 关键逻辑：SIGKILL（终止信号）不能被阻塞，
// 因此通过& ~(1 << (SIGKILL - 1))强制清除新掩码中 SIGKILL 的阻塞位。
int sys_ssetmask(int newmask)
{
    int old = current->blocked; // 保存旧的信号掩码

    // 设置新的信号掩码，但强制不能阻塞SIGKILL（1 << (SIGKILL - 1)是SIGKILL的位掩码）
    current->blocked = newmask & ~(1 << (SIGKILL - 1));
    return old; // 返回旧的信号掩码
}

/**
 * func descp: 保存旧的信号处理结构（save_old）
 */
// 静态内联函数：将内核空间的信号处理结构（struct sigaction）复制到用户空间。
// verify_area：检查用户空间地址的合法性（内核安全机制）。
// put_fs_byte：内核向用户空间写入数据的函数（因内核和用户空间地址空间分离，需特殊处理）。
static inline void save_old(char *from, char *to)
{
    int i;

    // 验证用户空间地址to的内存是否可写（避免内核访问非法用户地址）
    verify_area(to, sizeof(struct sigaction));
    // 逐字节将内核空间的from复制到用户空间的to（信号处理结构）
    for (i = 0; i < sizeof(struct sigaction); i++)
    {
        put_fs_byte(*from, to); // 将内核数据写入用户空间
        from++;
        to++;
    }
}
/**
 * func descp: 获取新的信号处理结构（get_new）
 */

// 静态内联函数：将用户空间定义的信号处理结构复制到内核空间。
// get_fs_byte：从用户空间读取数据到内核的函数（与put_fs_byte对应）。
static inline void get_new(char *from, char *to)
{
    int i;

    // 逐字节将用户空间的from复制到内核空间的to（信号处理结构）
    for (i = 0; i < sizeof(struct sigaction); i++)
        *(to++) = get_fs_byte(from++); // 从用户空间读取数据到内核
}

/**
 * func descp: 注册信号处理函数（sys_signal）
 */

// 系统调用：注册信号处理函数（早期的signal()系统调用实现）。
// 关键标志：
// SA_ONESHOT：信号处理函数执行一次后自动失效（需重新注册）。
// SA_NOMASK：信号处理期间不阻塞自身（允许嵌套触发同一信号）。
int sys_signal(int signum, long handler, long restorer)
{
    struct sigaction tmp; // 临时信号处理结构

    // 检查信号编号合法性：1~32之间，且不能是SIGKILL（SIGKILL不能被捕获/修改）
    if (signum < 1 || signum > 32 || signum == SIGKILL)
        return -1;

    // 初始化临时信号处理结构
    tmp.sa_handler = (void (*)(int))handler;    // 信号处理函数（用户提供）
    tmp.sa_mask = 0;                            // 信号处理期间不阻塞其他信号
    tmp.sa_flags = SA_ONESHOT | SA_NOMASK;      // 标志：一次有效+不自动阻塞自身
    tmp.sa_restorer = (void (*)(void))restorer; // 信号处理完成后恢复现场的函数

    // 保存旧的信号处理函数（用于返回给用户）
    handler = (long)current->sigaction[signum - 1].sa_handler;
    // 更新当前进程的信号处理结构
    current->sigaction[signum - 1] = tmp;
    return handler; // 返回旧的信号处理函数地址
}

/**
 * func descp: 高级信号处理设置（sys_sigaction）
 */

// 系统调用：更灵活的信号处理设置（对应标准的sigaction()系统调用）。
// 功能：允许用户自定义信号处理函数、掩码和标志，比sys_signal更强大。
// 关键逻辑：自动处理信号掩码（若未设置SA_NOMASK，则信号处理期间自动阻塞自身）。

int sys_sigaction(int signum, const struct sigaction *action,
                  struct sigaction *oldaction)
{
    struct sigaction tmp; // 临时保存旧的信号处理结构

    // 检查信号编号合法性（同sys_signal）
    if (signum < 1 || signum > 32 || signum == SIGKILL)
        return -1;

    // 保存当前信号的旧处理结构
    tmp = current->sigaction[signum - 1];
    // 将用户提供的新处理结构从用户空间复制到内核（更新当前进程的信号处理）
    get_new((char *)action, (char *)(signum - 1 + current->sigaction));

    // 如果需要返回旧处理结构，将其从内核复制到用户空间
    if (oldaction)
        save_old((char *)&tmp, (char *)oldaction);

    // 根据标志更新信号掩码：
    // 如果设置了SA_NOMASK，则不阻塞任何信号；否则自动阻塞当前信号
    if (current->sigaction[signum - 1].sa_flags & SA_NOMASK)
        current->sigaction[signum - 1].sa_mask = 0;
    else
        current->sigaction[signum - 1].sa_mask |= (1 << (signum - 1));

    return 0; // 成功
}

/**
 * func descp:  信号处理核心逻辑（do_signal）
 */

// 核心函数：当内核检测到进程有未处理的信号时，调用此函数处理。
// 执行流程：
// 检查信号处理函数：忽略（1）、默认（NULL）或用户自定义。
// 若为一次性处理（SA_ONESHOT），清空处理函数。
// 修改指令指针（eip），让进程从用户态的信号处理函数开始执行。
// 保存现场：将寄存器、原指令地址等压入用户栈，供处理完成后恢复。
// 阻塞sa_mask中指定的信号，避免处理期间被干扰。

void do_signal(long signr, long eax, long ebx, long ecx, long edx,
               long fs, long es, long ds,
               long eip, long cs, long eflags,
               unsigned long *esp, long ss)
{
    unsigned long sa_handler; // 信号处理函数地址
    long old_eip = eip;       // 保存原指令地址（信号处理完成后返回）
    // 获取当前信号对应的处理结构
    struct sigaction *sa = current->sigaction + signr - 1;
    int longs;              // 栈上需要保存的长整型数据个数
    unsigned long *tmp_esp; // 临时栈指针

    sa_handler = (unsigned long)sa->sa_handler; // 获取处理函数地址

    // 处理函数为1：忽略信号（SIG_IGN）
    if (sa_handler == 1)
        return;

    // 处理函数为NULL：使用默认处理
    if (!sa_handler)
    {
        if (signr == SIGCHLD) // SIGCHLD（子进程状态变化）默认忽略
            return;
        else // 其他信号默认处理：进程退出
            do_exit(1 << (signr - 1));
    }

    // 如果是一次性处理（SA_ONESHOT），执行后清空处理函数（下次用默认行为）
    if (sa->sa_flags & SA_ONESHOT)
        sa->sa_handler = NULL;

    // 修改指令指针：跳转到信号处理函数（用户态执行）
    *(&eip) = sa_handler;

    // 计算需要保存到栈的参数个数：SA_NOMASK则7个，否则8个（多保存一个信号掩码）
    longs = (sa->sa_flags & SA_NOMASK) ? 7 : 8;
    // 调整用户栈指针：为保存参数预留空间
    *(&esp) -= longs;
    verify_area(esp, longs * 4); // 验证栈空间合法性
    tmp_esp = esp;               // 临时栈指针

    // 将以下数据依次压入用户栈（供信号处理函数和恢复现场使用）
    put_fs_long((long)sa->sa_restorer, tmp_esp++); // 恢复现场函数地址
    put_fs_long(signr, tmp_esp++);                 // 信号编号
    if (!(sa->sa_flags & SA_NOMASK))
        put_fs_long(current->blocked, tmp_esp++); // 信号掩码（若需要）
    put_fs_long(eax, tmp_esp++);                  // 寄存器eax
    put_fs_long(ecx, tmp_esp++);                  // 寄存器ecx
    put_fs_long(edx, tmp_esp++);                  // 寄存器edx
    put_fs_long(eflags, tmp_esp++);               // 标志寄存器
    put_fs_long(old_eip, tmp_esp++);              // 原指令地址（处理完成后返回）

    // 信号处理期间阻塞sa_mask中指定的信号
    current->blocked |= sa->sa_mask;
}