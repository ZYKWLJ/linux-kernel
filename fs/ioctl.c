/*
 *  linux/fs/ioctl.c
 *
 *  (C) 1991  Linus Torvalds
 *  该文件实现了ioctl系统调用，用于对设备进行各种控制操作
 */

/* #include <string.h>*/
#include <errno.h>         // 包含错误码定义
#include <sys/stat.h>      // 包含文件状态相关宏定义

#include <linux/sched.h>   // 包含进程调度相关定义

// 声明tty设备的ioctl处理函数（在其他文件中实现）
extern int tty_ioctl(int dev, int cmd, int arg);

// 定义ioctl处理函数指针类型，指向特定设备的ioctl处理函数
// 参数：设备号、命令、参数；返回值：操作结果
typedef int (*ioctl_ptr)(int dev, int cmd, int arg);

// 计算设备类型数量：ioctl_table数组元素个数 = 数组总大小 / 单个元素大小
#define NRDEVS ((sizeof(ioctl_table)) / (sizeof(ioctl_ptr)))

// ioctl处理函数表：每种主设备号对应一个处理函数
// 数组索引对应主设备号，值为该设备类型的ioctl处理函数
static ioctl_ptr ioctl_table[] = {
    NULL,      /* nodev - 无设备（主设备号0） */
    NULL,      /* /dev/mem - 内存设备（主设备号1） */
    NULL,      /* /dev/fd - 文件描述符设备（主设备号2） */
    NULL,      /* /dev/hd - 硬盘设备（主设备号3） */
    tty_ioctl, /* /dev/ttyx - 终端设备（主设备号4） */
    tty_ioctl, /* /dev/tty - 控制终端（主设备号5） */
    NULL,      /* /dev/lp - 打印机设备（主设备号6） */
    NULL};     /* named pipes - 命名管道（主设备号7） */

/*
 * sys_ioctl系统调用：设备控制操作
 * @fd: 文件描述符
 * @cmd: 控制命令
 * @arg: 命令参数
 * 功能：根据文件描述符找到对应的设备，调用该设备的ioctl处理函数
 * 返回值：成功返回处理结果，失败返回错误码
 */
int sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
    struct file *filp;  // 文件结构体指针
    int dev, mode;      // dev-设备号；mode-文件类型和权限

    // 检查文件描述符有效性：不能超过最大打开文件数，且必须指向一个打开的文件
    if (fd >= NR_OPEN || !(filp = current->filp[fd]))
        return -EBADF;  // 返回"错误的文件描述符"错误
    // 获取文件的类型和权限信息
    mode = filp->f_inode->i_mode;
    // 检查文件是否为字符设备或块设备（只有设备文件才支持ioctl操作）
    if (!S_ISCHR(mode) && !S_ISBLK(mode))
        return -EINVAL;  // 返回"无效的参数"错误（非设备文件不支持）
    // 获取设备号（存储在inode的i_zone[0]中）
    dev = filp->f_inode->i_zone[0];
    // 检查主设备号是否超出支持的范围
    if (MAJOR(dev) >= NRDEVS)
        return -ENODEV;  // 返回"设备不存在"错误
    // 检查该主设备号是否有对应的ioctl处理函数
    if (!ioctl_table[MAJOR(dev)])
        return -ENOTTY;  // 返回"不是终端设备"错误（此处泛指不支持ioctl）
    // 调用对应设备的ioctl处理函数，并返回结果
    return ioctl_table[MAJOR(dev)](dev, cmd, arg);
}