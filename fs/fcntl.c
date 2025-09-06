/*
 *  linux/fs/fcntl.c
 *
 *  (C) 1991  Linus Torvalds
 *  该文件实现了文件控制相关的系统调用，包括dup、dup2和fcntl等
 */

/* #include <string.h> */
#include <errno.h>        // 包含错误码定义
#include <linux/sched.h>  // 包含进程调度相关定义
#include <linux/kernel.h> // 包含内核核心函数定义
#include <asm/segment.h>  // 包含段操作相关函数定义

#include <fcntl.h>    // 包含文件控制相关宏定义
#include <sys/stat.h> // 包含文件状态相关定义

extern int sys_close(int fd); // 声明关闭文件描述符的系统调用

/*
 * 复制文件描述符
 * @fd: 要复制的源文件描述符
 * @arg: 新文件描述符的最小值（从该值开始查找可用的描述符）
 * 功能：创建一个新的文件描述符，指向与源描述符相同的文件
 * 返回值：成功返回新的文件描述符，失败返回错误码
 */
static int dupfd(unsigned int fd, unsigned int arg)
{
    // 检查源文件描述符的有效性：不能超过最大打开文件数，且必须指向一个打开的文件
    if (fd >= NR_OPEN || !current->filp[fd])
        return -EBADF; // 返回"错误的文件描述符"错误

    // 检查请求的新描述符最小值是否超过最大限制
    if (arg >= NR_OPEN)
        return -EINVAL; // 返回"无效的参数"错误

    // 从arg开始查找第一个未使用的文件描述符
    while (arg < NR_OPEN)
        if (current->filp[arg]) // 如果该描述符已被使用，则继续查找下一个
            arg++;
        else
            break; // 找到可用的描述符，退出循环

    // 如果没有找到可用的文件描述符（所有描述符都已用完）
    if (arg >= NR_OPEN)
        return -EMFILE; // 返回"打开的文件过多"错误

    // 清除新描述符的执行时关闭标志
    current->close_on_exec &= ~(1 << arg);
    // 将新描述符指向源描述符对应的文件结构体，并增加文件引用计数
    (current->filp[arg] = current->filp[fd])->f_count++;

    return arg; // 返回新的文件描述符
}

/*
 * sys_dup2系统调用：复制文件描述符到指定的新描述符
 * @oldfd: 源文件描述符
 * @newfd: 目标文件描述符
 * 功能：关闭newfd（如果已打开），然后将oldfd复制到newfd
 * 返回值：成功返回newfd，失败返回错误码
 */
int sys_dup2(unsigned int oldfd, unsigned int newfd)
{
    // 先关闭新的文件描述符（如果它已经打开）
    sys_close(newfd);
    // 然后将源描述符复制到新描述符
    return dupfd(oldfd, newfd);
}

/*
 * sys_dup系统调用：复制文件描述符
 * @fildes: 要复制的源文件描述符
 * 功能：创建一个新的文件描述符，指向与源描述符相同的文件，新描述符是最小的可用值
 * 返回值：成功返回新的文件描述符，失败返回错误码
 */
int sys_dup(unsigned int fildes)
{
    // 调用dupfd，从0开始查找可用的新描述符
    return dupfd(fildes, 0);
}

/*
 * sys_fcntl系统调用：文件控制操作
 * @fd: 文件描述符
 * @cmd: 要执行的控制命令
 * @arg: 命令的参数
 * 功能：对文件描述符执行各种控制操作，如复制描述符、获取/设置标志等
 * 返回值：根据不同命令返回不同结果，失败返回错误码
 */
int sys_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
    struct file *filp; // 文件结构体指针

    // 检查文件描述符的有效性
    if (fd >= NR_OPEN || !(filp = current->filp[fd]))
        return -EBADF; // 返回"错误的文件描述符"错误

    // 根据命令类型执行不同的操作
    switch (cmd)
    {
    case F_DUPFD: // 复制文件描述符
        return dupfd(fd, arg);

    case F_GETFD: // 获取文件描述符标志（close-on-exec）
        // 返回该文件描述符的执行时关闭标志位
        return (current->close_on_exec >> fd) & 1;

    case F_SETFD:    // 设置文件描述符标志（close-on-exec）
        if (arg & 1) // 如果参数的最低位为1，则设置执行时关闭标志
            current->close_on_exec |= (1 << fd);
        else // 否则清除执行时关闭标志
            current->close_on_exec &= ~(1 << fd);
        return 0; // 成功返回0

    case F_GETFL:             // 获取文件状态标志
        return filp->f_flags; // 返回文件结构体中的标志

    case F_SETFL: // 设置文件状态标志
        // 先清除原来的O_APPEND和O_NONBLOCK标志
        filp->f_flags &= ~(O_APPEND | O_NONBLOCK);
        // 再设置新的O_APPEND和O_NONBLOCK标志（仅允许这两个标志被修改）
        filp->f_flags |= arg & (O_APPEND | O_NONBLOCK);
        return 0; // 成功返回0

    case F_GETLK:  // 获取文件锁（未实现）
    case F_SETLK:  // 设置文件锁（未实现）
    case F_SETLKW: // 设置文件锁（等待模式，未实现）
        return -1; // 返回错误

    default:       // 未知命令
        return -1; // 返回错误
    }
}