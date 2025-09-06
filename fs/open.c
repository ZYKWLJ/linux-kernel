/*
 * linux/fs/open.c
 * 文件系统打开操作相关系统调用实现
 * (C) 1991 Linus Torvalds
 */

/* #include <string.h> */ // 字符串头文件（已注释掉）
#include <errno.h>        // 错误号定义头文件
#include <fcntl.h>        // 文件控制头文件
#include <sys/types.h>    // 类型定义头文件
#include <utime.h>        // 时间函数头文件
#include <sys/stat.h>     // 文件状态头文件

#include <linux/sched.h>  // 内核调度程序头文件
#include <linux/tty.h>    // tty头文件
#include <linux/kernel.h> // 内核头文件
#include <asm/segment.h>  // 段操作头文件

// 系统调用：获取文件系统统计信息（未实现）
int sys_ustat(int dev, struct ustat *ubuf)
{
    return -ENOSYS; // 返回"功能未实现"错误
}

// 系统调用：设置文件访问和修改时间
int sys_utime(char *filename, struct utimbuf *times)
{
    struct m_inode *inode; // 内存inode指针
    long actime, modtime;  // 访问时间和修改时间

    // 通过文件名获取inode，失败返回"文件不存在"错误
    if (!(inode = namei(filename)))
        return -ENOENT;

    // 如果times指针不为空，从用户空间获取时间值
    if (times)
    {
        actime = get_fs_long((unsigned long *)&times->actime);   // 获取访问时间
        modtime = get_fs_long((unsigned long *)&times->modtime); // 获取修改时间
    }
    else
        actime = modtime = CURRENT_TIME; // 如果times为空，使用当前时间

    inode->i_atime = actime;  // 设置inode访问时间
    inode->i_mtime = modtime; // 设置inode修改时间
    inode->i_dirt = 1;        // 标记inode为脏（需要写回磁盘）
    iput(inode);              // 释放inode引用
    return 0;                 // 成功返回0
}

/*
 * 注释：我们应该使用真实用户ID还是有效用户ID？
 * BSD使用真实用户ID，这样可以使这个调用对setuid程序有用
 */

// 系统调用：检查文件访问权限
int sys_access(const char *filename, int mode)
{
    struct m_inode *inode; // 内存inode指针
    int res, i_mode;       // 结果和inode模式

    mode &= 0007; // 只保留最低3位（其他用户权限位）

    // 通过文件名获取inode，失败返回"权限拒绝"错误
    if (!(inode = namei(filename)))
        return -EACCES;

    i_mode = res = inode->i_mode & 0777; // 获取inode权限位
    iput(inode);                         // 释放inode引用

    // 如果当前进程用户ID等于文件所有者ID，检查所有者权限位
    if (current->uid == inode->i_uid)
        res >>= 6; // 右移6位，获取所有者权限
    // 如果当前进程组ID等于文件组ID，检查组权限位
    else if (current->gid == inode->i_gid)
        res >>= 3; // 右移3位，获取组权限

    // 检查权限是否匹配
    if ((res & 0007 & mode) == mode)
        return 0; // 权限足够，返回成功

    /*
     * 注释：我们最后做这个测试，因为我们真的应该
     * 交换有效用户ID和真实用户ID（临时地），
     * 然后调用suser()例程。如果我们调用suser()例程，
     * 它需要在最后调用
     */

    // 如果是超级用户，并且（不需要执行权限 或者 文件有执行权限）
    if ((!current->uid) &&
        (!(mode & 1) || (i_mode & 0111)))
        return 0; // 允许访问

    return -EACCES; // 权限不足，返回错误
}

// 系统调用：改变当前工作目录
int sys_chdir(const char *filename)
{
    struct m_inode *inode; // 内存inode指针

    // 通过文件名获取inode，失败返回"文件不存在"错误
    if (!(inode = namei(filename)))
        return -ENOENT;

    // 检查是否为目录文件
    if (!S_ISDIR(inode->i_mode))
    {
        iput(inode);     // 释放inode引用
        return -ENOTDIR; // 返回"不是目录"错误
    }

    iput(current->pwd);   // 释放当前工作目录inode
    current->pwd = inode; // 设置新的当前工作目录
    return (0);           // 成功返回0
}

// 系统调用：改变根目录
int sys_chroot(const char *filename)
{
    struct m_inode *inode; // 内存inode指针

    // 通过文件名获取inode，失败返回"文件不存在"错误
    if (!(inode = namei(filename)))
        return -ENOENT;

    // 检查是否为目录文件
    if (!S_ISDIR(inode->i_mode))
    {
        iput(inode);     // 释放inode引用
        return -ENOTDIR; // 返回"不是目录"错误
    }

    iput(current->root);   // 释放当前根目录inode
    current->root = inode; // 设置新的根目录
    return (0);            // 成功返回0
}

// 系统调用：改变文件模式（权限）
int sys_chmod(const char *filename, int mode)
{
    struct m_inode *inode; // 内存inode指针

    // 通过文件名获取inode，失败返回"文件不存在"错误
    if (!(inode = namei(filename)))
        return -ENOENT;

    // 检查权限：如果不是文件所有者且不是超级用户
    if ((current->euid != inode->i_uid) && !suser())
    {
        iput(inode);    // 释放inode引用
        return -EACCES; // 返回"权限拒绝"错误
    }

    // 设置新的文件模式，保留文件类型位
    inode->i_mode = (mode & 07777) | (inode->i_mode & ~07777);
    inode->i_dirt = 1; // 标记inode为脏
    iput(inode);       // 释放inode引用
    return 0;          // 成功返回0
}

// 系统调用：改变文件所有者和组
int sys_chown(const char *filename, int uid, int gid)
{
    struct m_inode *inode; // 内存inode指针

    // 通过文件名获取inode，失败返回"文件不存在"错误
    if (!(inode = namei(filename)))
        return -ENOENT;

    // 检查是否为超级用户
    if (!suser())
    {
        iput(inode);    // 释放inode引用
        return -EACCES; // 返回"权限拒绝"错误
    }

    inode->i_uid = uid; // 设置用户ID
    inode->i_gid = gid; // 设置组ID
    inode->i_dirt = 1;  // 标记inode为脏
    iput(inode);        // 释放inode引用
    return 0;           // 成功返回0
}

// 系统调用：打开文件
int sys_open(const char *filename, int flag, int mode)
{
    struct m_inode *inode; // 内存inode指针
    struct file *f;        // 文件结构指针
    int i, fd;             // 循环变量和文件描述符

    // 调整模式位，去除umask屏蔽的权限
    mode &= 0777 & ~current->umask;

    // 寻找空闲的文件描述符
    for (fd = 0; fd < NR_OPEN; fd++)
        if (!current->filp[fd])
            break;

    // 如果没有空闲文件描述符，返回错误
    if (fd >= NR_OPEN)
        return -EINVAL;

    // 清除执行时关闭标志
    current->close_on_exec &= ~(1 << fd);

    // 在文件表中寻找空闲项
    f = 0 + file_table; // 指向文件表开始
    for (i = 0; i < NR_FILE; i++, f++)
        if (!f->f_count)
            break;

    // 如果没有空闲文件表项，返回错误
    if (i >= NR_FILE)
        return -EINVAL;

    // 设置文件描述符指向找到的文件表项，并增加引用计数
    (current->filp[fd] = f)->f_count++;

    // 调用open_namei进行路径名解析和inode获取
    if ((i = open_namei(filename, flag, mode, &inode)) < 0)
    {
        current->filp[fd] = NULL; // 清除文件描述符指针
        f->f_count = 0;           // 清除引用计数
        return i;                 // 返回错误码
    }

    /* tty设备有些特殊（ttyxx主设备号==4，tty主设备号==5） */
    if (S_ISCHR(inode->i_mode)) // 如果是字符设备
    {
        if (MAJOR(inode->i_zone[0]) == 4) // 主设备号为4（控制终端）
        {
            // 如果是进程组首领且还没有控制终端
            if (current->leader && current->tty < 0)
            {
                current->tty = MINOR(inode->i_zone[0]);       // 设置控制终端
                tty_table[current->tty].pgrp = current->pgrp; // 设置终端进程组
            }
        }
        else if (MAJOR(inode->i_zone[0]) == 5) // 主设备号为5（虚拟终端）
        {
            // 如果没有控制终端，拒绝打开
            if (current->tty < 0)
            {
                iput(inode);              // 释放inode
                current->filp[fd] = NULL; // 清除文件描述符指针
                f->f_count = 0;           // 清除引用计数
                return -EPERM;            // 返回"操作不被允许"错误
            }
        }
    }
    /* 同样处理块设备：检查软盘更换 */
    if (S_ISBLK(inode->i_mode))              // 如果是块设备
        check_disk_change(inode->i_zone[0]); // 检查磁盘是否更换
    // 设置文件结构字段
    f->f_mode = inode->i_mode; // 文件模式
    f->f_flags = flag;         // 文件打开标志
    f->f_count = 1;            // 引用计数
    f->f_inode = inode;        // 指向inode
    f->f_pos = 0;              // 文件位置指针

    return (fd); // 返回文件描述符
}

// 系统调用：创建文件
int sys_creat(const char *pathname, int mode)
{
    // 调用sys_open，使用O_CREAT和O_TRUNC标志
    return sys_open(pathname, O_CREAT | O_TRUNC, mode);
}

// 系统调用：关闭文件
int sys_close(unsigned int fd)
{
    struct file *filp; // 文件结构指针
    // 检查文件描述符有效性
    if (fd >= NR_OPEN)
        return -EINVAL;
    // 清除执行时关闭标志
    current->close_on_exec &= ~(1 << fd);
    // 获取文件结构指针，检查是否有效
    if (!(filp = current->filp[fd]))
        return -EINVAL;
    current->filp[fd] = NULL; // 清除文件描述符指针
    // 如果引用计数已经是0，这是严重错误
    if (filp->f_count == 0)
        panic("Close: file count is 0");
    // 减少引用计数，如果还有引用，直接返回
    if (--filp->f_count)
        return (0);
    iput(filp->f_inode); // 释放inode引用
    return (0);          // 成功返回0
}