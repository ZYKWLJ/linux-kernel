/*
 *  linux/kernel/sys.c
 *
 *  (C) 1991  Linus Torvalds  // 代码作者及年份
 *  该文件实现了Linux内核中的部分系统调用处理函数
 */

#include <errno.h> // 包含错误码定义

#include <linux/sched.h>  // 包含进程调度相关结构体定义
#include <linux/tty.h>    // 包含终端设备相关定义
#include <linux/kernel.h> // 包含内核通用函数定义
#include <asm/segment.h>  // 包含段操作相关函数（用户空间与内核空间数据传递）
#include <sys/times.h>    // 包含times系统调用相关结构体定义
#include <sys/utsname.h>  // 包含uname系统调用相关结构体定义

// 未实现的ftime系统调用：返回不支持该系统调用的错误
int sys_ftime()
{
    return -ENOSYS; // ENOSYS表示"函数未实现"
}

// 未实现的break系统调用
int sys_break()
{
    return -ENOSYS;
}

// 未实现的ptrace系统调用（用于进程调试）
int sys_ptrace()
{
    return -ENOSYS;
}

// 未实现的stty系统调用（设置终端属性）
int sys_stty()
{
    return -ENOSYS;
}

// 未实现的gtty系统调用（获取终端属性）
int sys_gtty()
{
    return -ENOSYS;
}

// 未实现的rename系统调用（重命名文件）
int sys_rename()
{
    return -ENOSYS;
}

// 未实现的prof系统调用（程序性能分析）
int sys_prof()
{
    return -ENOSYS;
}

/*
 * 设置实际组ID和有效组ID
 * rgid: 新的实际组ID（若为0则不修改）
 * egid: 新的有效组ID（若为0则不修改）
 */
int sys_setregid(int rgid, int egid)
{
    // 修改实际组ID
    if (rgid > 0)
    {
        // 只有当当前实际组ID等于新ID，或者是超级用户时才允许修改
        if ((current->gid == rgid) || suser())
            current->gid = rgid; // current是当前进程结构体指针
        else
            return -EPERM; // EPERM表示"操作不允许"
    }

    // 修改有效组ID
    if (egid > 0)
    {
        // 允许修改的条件：当前实际组ID、有效组ID、保存的组ID与新ID相同，或为超级用户
        if ((current->gid == egid) ||
            (current->egid == egid) ||
            (current->sgid == egid) ||
            suser())
            current->egid = egid;
        else
            return -EPERM;
    }
    return 0; // 成功执行
}

/*
 * 设置组ID（实际组ID和有效组ID同时设置为相同值）
 * 直接调用sys_setregid实现
 */
int sys_setgid(int gid)
{
    return sys_setregid(gid, gid);
}

// 未实现的acct系统调用（进程记账功能）
int sys_acct()
{
    return -ENOSYS;
}

// 未实现的phys系统调用
int sys_phys()
{
    return -ENOSYS;
}

// 未实现的lock系统调用（文件锁定）
int sys_lock()
{
    return -ENOSYS;
}

// 未实现的mpx系统调用（内存保护扩展）
int sys_mpx()
{
    return -ENOSYS;
}

// 未实现的ulimit系统调用（设置用户资源限制）
int sys_ulimit()
{
    return -ENOSYS;
}

/*
 * 获取当前时间
 * tloc: 用户空间指针，用于存储当前时间（若不为NULL）
 * 返回当前时间（从1970年1月1日开始的秒数）
 */
int sys_time(long *tloc)
{
    int i;

    i = CURRENT_TIME; // 获取当前时间（内核宏定义）
    if (tloc)
    {
        verify_area(tloc, 4);                  // 验证用户空间地址是否可写（安全性检查）
        put_fs_long(i, (unsigned long *)tloc); // 将时间写入用户空间
    }
    return i;
}

/*
 * 设置实际用户ID和有效用户ID
 * ruid: 新的实际用户ID（若为0则不修改）
 * euid: 新的有效用户ID（若为0则不修改）
 * 注：非特权用户可以将实际用户ID与有效用户ID互换
 */
int sys_setreuid(int ruid, int euid)
{
    int old_ruid = current->uid; // 保存原始实际用户ID

    // 修改实际用户ID
    if (ruid > 0)
    {
        // 允许修改的条件：有效用户ID等于新ID、原始实际ID等于新ID，或为超级用户
        if ((current->euid == ruid) ||
            (old_ruid == ruid) ||
            suser())
            current->uid = ruid;
        else
            return -EPERM;
    }

    // 修改有效用户ID
    if (euid > 0)
    {
        // 允许修改的条件：原始实际ID等于新ID、有效用户ID等于新ID，或为超级用户
        if ((old_ruid == euid) ||
            (current->euid == euid) ||
            suser())
            current->euid = euid;
        else
        {
            current->uid = old_ruid; // 恢复原始实际用户ID
            return -EPERM;
        }
    }
    return 0;
}

/*
 * 设置用户ID（实际用户ID和有效用户ID同时设置为相同值）
 * 直接调用sys_setreuid实现
 */
int sys_setuid(int uid)
{
    return sys_setreuid(uid, uid);
}

/*
 * 设置系统时间
 * tptr: 指向新时间的用户空间指针
 * 只有超级用户可以设置系统时间
 */
int sys_stime(long *tptr)
{
    if (!suser()) // 检查是否为超级用户
        return -EPERM;
    // 计算启动时间：新时间减去当前系统运行的秒数
    startup_time = get_fs_long((unsigned long *)tptr) - jiffies / HZ;
    return 0;
}

/*
 * 获取进程时间信息
 * tbuf: 指向tms结构体的用户空间指针，用于存储时间信息
 * 返回从系统启动开始的滴答数（jiffies）
 */
int sys_times(struct tms *tbuf)
{
    if (tbuf)
    {
        verify_area(tbuf, sizeof(*tbuf)); // 验证用户空间地址
        // 将进程时间信息写入用户空间
        put_fs_long(current->utime, (unsigned long *)&tbuf->tms_utime);   // 用户态时间
        put_fs_long(current->stime, (unsigned long *)&tbuf->tms_stime);   // 内核态时间
        put_fs_long(current->cutime, (unsigned long *)&tbuf->tms_cutime); // 子进程用户态时间
        put_fs_long(current->cstime, (unsigned long *)&tbuf->tms_cstime); // 子进程内核态时间
    }
    return jiffies; // 返回系统启动后的滴答数
}

/*
 * 调整进程数据段大小（brk系统调用）
 * end_data_seg: 新的数据段结束地址
 * 返回调整后的实际数据段结束地址
 */
int sys_brk(unsigned long end_data_seg)
{
    // 检查新地址是否在合法范围内（代码段结束之后，栈开始之前，保留16KB空间）
    if (end_data_seg >= current->end_code &&
        end_data_seg < current->start_stack - 16384)
        current->brk = end_data_seg; // 更新数据段结束地址
    return current->brk;             // 返回当前数据段结束地址
}

/*
 * 设置进程组ID
 * pid: 要修改的进程ID（0表示当前进程）
 * pgid: 新的进程组ID（0表示使用进程ID作为组ID）
 * 注：该实现需要更多检查，目前对会话/进程组等概念理解有限
 */
int sys_setpgid(int pid, int pgid)
{
    int i;

    if (!pid)
        pid = current->pid; // 0表示当前进程
    if (!pgid)
        pgid = current->pid; // 0表示使用进程ID作为组ID

    // 遍历任务数组查找指定进程
    for (i = 0; i < NR_TASKS; i++)
        if (task[i] && task[i]->pid == pid)
        {
            if (task[i]->leader) // 进程组领导者不能改变组ID
                return -EPERM;
            if (task[i]->session != current->session) // 必须同一会话
                return -EPERM;
            task[i]->pgrp = pgid; // 设置新的进程组ID
            return 0;
        }
    return -ESRCH; // 未找到进程
}

/*
 * 获取当前进程的进程组ID
 */
int sys_getpgrp(void)
{
    return current->pgrp;
}

/*
 * 创建新的会话并设置进程组
 * 成功返回新的会话ID（等于进程组ID和进程ID）
 */
int sys_setsid(void)
{
    // 进程组领导者且非超级用户不能创建新会话
    if (current->leader && !suser())
        return -EPERM;
    current->leader = 1;                             // 标记为会话领导者
    current->session = current->pgrp = current->pid; // 会话ID、进程组ID均设为进程ID
    current->tty = -1;                               // 脱离终端控制
    return current->pgrp;
}

/*
 * 获取系统信息（uname系统调用）
 * name: 指向utsname结构体的用户空间指针，用于存储系统信息
 */
int sys_uname(struct utsname *name)
{
    // 静态系统信息结构体（Linux 0.11版本信息）
    static struct utsname thisname = {
        "linux .0", // 系统名称和版本
        "nodename", // 节点名称（主机名）
        "release ", // 内核发行版本
        "version ", // 内核版本
        "machine "  // 硬件架构
    };
    int i;

    if (!name)
        return -ERROR;                // 无效指针
    verify_area(name, sizeof(*name)); // 验证用户空间地址
    // 将系统信息复制到用户空间
    for (i = 0; i < sizeof(*name); i++)
        put_fs_byte(((char *)&thisname)[i], i + (char *)name);
    return 0;
}

/*
 * 设置文件创建掩码（umask）
 * mask: 新的掩码值（只保留低9位，即0777）
 * 返回旧的掩码值
 */
int sys_umask(int mask)
{
    int old = current->umask; // 保存旧掩码

    current->umask = mask & 0777; // 设置新掩码（只保留权限相关位）
    return old;                   // 返回旧掩码
}