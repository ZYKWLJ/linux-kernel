/*
 *  linux/fs/read_write.c
 *
 *  (C) 1991  Linus Torvalds
 *
 *  功能说明：Linux 0.11 内核中读写操作的系统调用实现
 *  包含sys_lseek（文件偏移定位）、sys_read（读操作）、sys_write（写操作）三个核心系统调用，
 *  负责根据文件类型（管道、字符设备、块设备、普通文件等）分发到对应的底层处理函数，
 *  是用户程序与内核文件操作之间的关键接口层
 */

#include <sys/stat.h>  // 文件状态相关定义（如S_ISCHR、S_ISBLK等宏）
#include <errno.h>     // 错误码定义（如EBADF、EINVAL等）
#include <sys/types.h> // 基本数据类型定义（如off_t偏移量类型）

#include <linux/kernel.h> // 内核核心函数（如printk）
#include <linux/sched.h>  // 进程相关结构（如current进程、file结构、inode结构）
#include <asm/segment.h>  // 段操作函数（如verify_area检查用户空间内存）

// 外部声明：各类型文件的底层读写函数
extern int rw_char(int rw, int dev, char *buf, int count, off_t *pos);                 // 字符设备读写
extern int read_pipe(struct m_inode *inode, char *buf, int count);                     // 管道读操作
extern int write_pipe(struct m_inode *inode, char *buf, int count);                    // 管道写操作
extern int block_read(int dev, off_t *pos, char *buf, int count);                      // 块设备读操作
extern int block_write(int dev, off_t *pos, char *buf, int count);                     // 块设备写操作
extern int file_read(struct m_inode *inode, struct file *filp, char *buf, int count);  // 普通文件读
extern int file_write(struct m_inode *inode, struct file *filp, char *buf, int count); // 普通文件写

/**
 * @brief 系统调用sys_lseek：调整文件读写偏移量（定位操作）
 * @param fd 文件描述符
 * @param offset 偏移量（字节数）
 * @param origin 偏移起始位置（0=文件开头，1=当前位置，2=文件末尾）
 * @return 成功：新的偏移量；失败：负错误码
 */
int sys_lseek(unsigned int fd, off_t offset, int origin)
{
    struct file *file; // 文件结构指针
    int tmp;           // 临时变量（用于计算基于文件末尾的偏移）

    // 参数合法性检查：
    // 1. 文件描述符超出范围 2. 文件描述符未关联文件 3. 文件无inode 4. 设备不可定位
    if (fd >= NR_OPEN || !(file = current->filp[fd]) || !(file->f_inode) 
                      || !IS_SEEKABLE(MAJOR(file->f_inode->i_dev)))
        return -EBADF; // 错误：无效文件描述符（Bad file descriptor）
    // 管道不可定位（管道是流式设备，无固定大小）
    if (file->f_inode->i_pipe)
        return -ESPIPE; // 错误：非法定位操作（Illegal seek）

    // 根据origin计算新的偏移量
    switch (origin)
    {
    case 0: // 从文件开头开始（SEEK_SET）
        if (offset < 0)
            return -EINVAL; // 偏移量不能为负
        file->f_pos = offset;
        break;
    case 1: // 从当前位置开始（SEEK_CUR）
        if (file->f_pos + offset < 0)
            return -EINVAL; // 计算后偏移量不能为负
        file->f_pos += offset;
        break;
    case 2: // 从文件末尾开始（SEEK_END）
        tmp = file->f_inode->i_size + offset;
        if (tmp < 0)
            return -EINVAL; // 计算后偏移量不能为负
        file->f_pos = tmp;
        break;
    default:            // 无效的origin参数
        return -EINVAL; // 错误：无效参数（Invalid argument）
    }
    return file->f_pos; // 返回新的偏移量
}

/**
 * @brief 系统调用sys_read：读取文件数据到用户空间缓冲区
 * @param fd 文件描述符
 * @param buf 用户空间缓冲区（存放读取结果）
 * @param count 期望读取的字节数
 * @return 成功：实际读取的字节数；失败：负错误码
 */
int sys_read(unsigned int fd, char *buf, int count)
{
    struct file *file;     // 文件结构指针
    struct m_inode *inode; // 索引节点指针

    // 参数合法性检查：
    // 1. 文件描述符超出范围 2. 读取字节数为负 3. 文件描述符未关联文件
    if (fd >= NR_OPEN || count < 0 || !(file = current->filp[fd]))
        return -EINVAL; // 错误：无效参数
    if (!count)
        return 0; // 读取0字节，直接返回0
    // 检查用户空间缓冲区是否可写（避免访问非法内存）
    verify_area(buf, count);
    // 获取文件对应的inode（存储文件元数据）
    inode = file->f_inode;

    // 根据文件类型分发到对应的读函数
    if (inode->i_pipe) // 管道文件
        // 检查文件是否以读模式打开（f_mode&1表示可读）
        return (file->f_mode & 1) ? read_pipe(inode, buf, count) : -EIO;
    if (S_ISCHR(inode->i_mode)) // 字符设备文件
        // 调用字符设备读写函数（READ标识），设备号存于inode->i_zone[0]
        return rw_char(READ, inode->i_zone[0], buf, count, &file->f_pos);
    if (S_ISBLK(inode->i_mode)) // 块设备文件
        // 调用块设备读函数，设备号存于inode->i_zone[0]
        return block_read(inode->i_zone[0], &file->f_pos, buf, count);
    if (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)) // 目录文件或普通文件
    {
        // 若读取超出文件大小，调整读取字节数为剩余可读取部分
        if (count + file->f_pos > inode->i_size)
            count = inode->i_size - file->f_pos;
        if (count <= 0)
            return 0; // 已到文件末尾，无数据可读
        // 调用普通文件读函数
        return file_read(inode, file, buf, count);
    }
    // 未知文件类型（错误处理）
    printk("(Read)inode->i_mode=%06o\n\r", inode->i_mode);
    return -EINVAL;
}

/**
 * @brief 系统调用sys_write：将用户空间缓冲区数据写入文件
 * @param fd 文件描述符
 * @param buf 用户空间缓冲区（存放待写入数据）
 * @param count 期望写入的字节数
 * @return 成功：实际写入的字节数；失败：负错误码
 */
int sys_write(unsigned int fd, char *buf, int count)
{
    struct file *file;     // 文件结构指针
    struct m_inode *inode; // 索引节点指针

    // 参数合法性检查：
    // 1. 文件描述符超出范围 2. 写入字节数为负 3. 文件描述符未关联文件
    if (fd >= NR_OPEN || count < 0 || !(file = current->filp[fd]))
        return -EINVAL; // 错误：无效参数
    if (!count)
        return 0; // 写入0字节，直接返回0
    // 获取文件对应的inode
    inode = file->f_inode;

    // 根据文件类型分发到对应的写函数
    if (inode->i_pipe) // 管道文件
        // 检查文件是否以写模式打开（f_mode&2表示可写）
        return (file->f_mode & 2) ? write_pipe(inode, buf, count) : -EIO;
    if (S_ISCHR(inode->i_mode)) // 字符设备文件
        // 调用字符设备读写函数（WRITE标识）
        return rw_char(WRITE, inode->i_zone[0], buf, count, &file->f_pos);
    if (S_ISBLK(inode->i_mode)) // 块设备文件
        // 调用块设备写函数
        return block_write(inode->i_zone[0], &file->f_pos, buf, count);
    if (S_ISREG(inode->i_mode)) // 普通文件
        // 调用普通文件写函数
        return file_write(inode, file, buf, count);
    // 未知文件类型（错误处理）
    printk("(Write)inode->i_mode=%06o\n\r", inode->i_mode);
    return -EINVAL;
}