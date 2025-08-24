/*
 *  linux/kernel/exit.c
 *
 *  (C) 1991  Linus Torvalds
 *  这是Linus Torvalds在1991年编写的Linux内核进程退出相关代码
 */

// 包含必要的头文件
#include <errno.h>    // 错误码定义
#include <signal.h>   // 信号相关定义
#include <sys/wait.h> // wait系统调用相关定义

#include <linux/sched.h>  // 进程调度相关结构定义
#include <linux/kernel.h> // 内核基本功能定义
#include <linux/tty.h>    // 终端设备相关定义
#include <asm/segment.h>  // 段操作相关函数

// 函数声明
int sys_pause(void);   // 声明pause系统调用
int sys_close(int fd); // 声明close系统调用

/**
 * 释放进程结构体所占用的内存
 * @param p 要释放的进程结构体指针
 */
void release(struct task_struct *p)
{
    int i;

    if (!p) // 如果进程指针为空，直接返回
        return;
    // 遍历所有任务槽位，查找要释放的进程
    for (i = 1; i < NR_TASKS; i++)
        if (task[i] == p)
        {                       // 找到匹配的进程
            task[i] = NULL;     // 清空任务槽位
            free_page((long)p); // 释放进程结构体占用的内存页
            schedule();         // 重新调度进程
            return;
        }
    // 如果没找到要释放的进程，触发内核恐慌
    panic("trying to release non-existent task");
}

/**
 * 向指定进程发送信号
 * @param sig 要发送的信号
 * @param p 目标进程结构体
 * @param priv 是否拥有特权
 * @return 0表示成功，错误码表示失败
 */
static inline int send_sig(long sig, struct task_struct *p, int priv)
{
    // 检查参数合法性：进程不存在或信号值无效
    if (!p || sig < 1 || sig > 32)
        return -EINVAL;

    // 检查权限：有特权，或同属一个用户，或超级用户
    if (priv || (current->euid == p->euid) || suser())
        p->signal |= (1 << (sig - 1)); // 设置对应信号位
    else
        return -EPERM; // 权限不足

    return 0;
}

/**
 * 向当前会话中的所有进程发送SIGHUP信号
 * 通常在会话 leader 退出时调用
 */
static void kill_session(void)
{
    struct task_struct **p = NR_TASKS + task; // 指向任务数组末尾的下一个位置

    // 遍历所有任务
    while (--p > &FIRST_TASK)
    {
        // 如果进程存在且属于当前会话，发送SIGHUP信号
        if (*p && (*p)->session == current->session)
            (*p)->signal |= 1 << (SIGHUP - 1);
    }
}

/*
 * XXX 需要检查向进程组等发送信号的权限
 * kill()的权限语义比较复杂!
 */

/**
 * kill系统调用实现：向指定进程或进程组发送信号
 * @param pid 进程ID或进程组ID（负数表示进程组）
 * @param sig 要发送的信号
 * @return 0表示成功，错误码表示失败
 */
int sys_kill(int pid, int sig)
{
    struct task_struct **p = NR_TASKS + task; // 指向任务数组末尾的下一个位置
    int err, retval = 0;

    // 根据pid的不同值处理不同情况
    if (!pid) // pid=0：向当前进程组的所有进程发送信号
        while (--p > &FIRST_TASK)
        {
            if (*p && (*p)->pgrp == current->pid)
                if ((err = send_sig(sig, *p, 1)))
                    retval = err;
        }
    else if (pid > 0) // pid>0：向指定pid的进程发送信号
        while (--p > &FIRST_TASK)
        {
            if (*p && (*p)->pid == pid)
                if ((err = send_sig(sig, *p, 0)))
                    retval = err;
        }
    else if (pid == -1) // pid=-1：向所有进程发送信号（除了init）
        while (--p > &FIRST_TASK)
        {
            if ((err = send_sig(sig, *p, 0)))
                retval = err;
        }
    else // pid<0且不为-1：向进程组ID为-pid的所有进程发送信号
        while (--p > &FIRST_TASK)
            if (*p && (*p)->pgrp == -pid)
                if ((err = send_sig(sig, *p, 0)))
                    retval = err;
    return retval;
}

/**
 * 通知父进程子进程状态变化（发送SIGCHLD信号）
 * @param pid 父进程ID
 */
static void tell_father(int pid)
{
    int i;

    if (pid)
        // 遍历所有任务查找父进程
        for (i = 0; i < NR_TASKS; i++)
        {
            if (!task[i])
                continue;
            if (task[i]->pid != pid)
                continue;
            // 找到父进程，发送SIGCHLD信号
            task[i]->signal |= (1 << (SIGCHLD - 1));
            return;
        }
    // 如果没找到父进程，释放当前进程
    /* 这不是很好的处理方式，应该改为让init进程(1号)作为父进程 */
    printk("BAD BAD - no father found\n\r");
    release(current);
}

/**
 * 处理进程退出的实际工作
 * @param code 退出码
 * @return 通常不会返回，-1仅用于抑制编译警告
 */
int do_exit(long code)
{
    int i;
    // 释放进程的代码段和数据段所占用的页表
    free_page_tables(get_base(current->ldt[1]), get_limit(0x0f));
    free_page_tables(get_base(current->ldt[2]), get_limit(0x17));

    // 处理子进程：将子进程的父进程改为init进程(1号)
    for (i = 0; i < NR_TASKS; i++)
        if (task[i] && task[i]->father == current->pid)
        {
            task[i]->father = 1;
            // 如果子进程已经是僵尸状态，通知init进程
            if (task[i]->state == TASK_ZOMBIE)
                /* 假设task[1]始终是init进程 */
                (void)send_sig(SIGCHLD, task[1], 1);
        }

    // 关闭所有打开的文件描述符
    for (i = 0; i < NR_OPEN; i++)
        if (current->filp[i])
            sys_close(i);

    // 释放文件系统相关的inode引用
    iput(current->pwd);
    current->pwd = NULL;
    iput(current->root);
    current->root = NULL;
    iput(current->executable);
    current->executable = NULL;

    // 如果是会话leader且有控制终端，释放终端
    if (current->leader && current->tty >= 0)
        tty_table[current->tty].pgrp = 0;

    // 如果当前进程使用了数学协处理器，清除记录
    if (last_task_used_math == current)
        last_task_used_math = NULL;

    // 如果是会话leader，终止会话中的所有进程
    if (current->leader)
        kill_session();

    // 将进程状态设置为僵尸状态，保存退出码
    current->state = TASK_ZOMBIE;
    current->exit_code = code;

    // 通知父进程
    tell_father(current->father);

    // 重新调度，让其他进程运行
    schedule();

    return (-1); /* 仅用于抑制编译警告 */
}

/**
 * exit系统调用实现
 * @param error_code 退出错误码
 * @return 不会实际返回
 */
int sys_exit(int error_code)
{
    // 将错误码处理后传递给do_exit
    return do_exit((error_code & 0xff) << 8);
}

/**
 * waitpid系统调用实现：等待子进程状态变化
 * @param pid 要等待的进程ID
 * @param stat_addr 存储子进程退出状态的地址
 * @param options 等待选项
 * @return 子进程ID，或错误码
 */
int sys_waitpid(pid_t pid, unsigned long *stat_addr, int options)
{
    int flag, code;
    struct task_struct **p;

    // 验证用户空间地址的可访问性
    verify_area(stat_addr, 4);

repeat:
    flag = 0;
    // 遍历所有任务查找符合条件的子进程
    for (p = &LAST_TASK; p > &FIRST_TASK; --p)
    {
        if (!*p || *p == current) // 跳过空进程和当前进程
            continue;
        if ((*p)->father != current->pid) // 只处理自己的子进程
            continue;

        // 根据pid参数筛选子进程
        if (pid > 0)
        {
            if ((*p)->pid != pid) // 等待特定pid的子进程
                continue;
        }
        else if (!pid)
        {
            if ((*p)->pgrp != current->pgrp) // 等待同一进程组的子进程
                continue;
        }
        else if (pid != -1)
        {
            if ((*p)->pgrp != -pid) // 等待特定进程组的子进程
                continue;
        }

        // 根据子进程状态处理
        switch ((*p)->state)
        {
        case TASK_STOPPED:              // 子进程已停止
            if (!(options & WUNTRACED)) // 如果不关注停止状态则跳过
                continue;
            put_fs_long(0x7f, stat_addr); // 存储停止状态码
            return (*p)->pid;             // 返回子进程ID
        case TASK_ZOMBIE:                 // 子进程已成为僵尸进程
            // 累加子进程的用户时间和系统时间
            current->cutime += (*p)->utime;
            current->cstime += (*p)->stime;
            flag = (*p)->pid;
            code = (*p)->exit_code;
            release(*p);                  // 释放子进程
            put_fs_long(code, stat_addr); // 存储退出码
            return flag;                  // 返回子进程ID
        default:
            flag = 1; // 标记有符合条件但未退出的子进程
            continue;
        }
    }

    // 处理等待逻辑
    if (flag)
    {
        if (options & WNOHANG) // 如果设置了WNOHANG，不阻塞直接返回0
            return 0;
        current->state = TASK_INTERRUPTIBLE; // 进入可中断等待状态
        schedule();                          // 调度其他进程运行
        // 如果被信号唤醒且不是SIGCHLD，返回中断错误
        if (!(current->signal &= ~(1 << (SIGCHLD - 1))))
            goto repeat; // 继续等待
        else
            return -EINTR;
    }
    return -ECHILD; // 没有符合条件的子进程
}