/*
 *  linux/fs/stat.c
 *
 *  (C) 1991  Linus Torvalds
 *  该文件实现了获取文件状态信息的系统调用
 */

#include <errno.h>         // 包含错误码定义
#include <sys/stat.h>      // 包含stat结构体定义

#include <linux/fs.h>      // 包含文件系统相关定义
#include <linux/sched.h>   // 包含进程调度相关定义
#include <linux/kernel.h>  // 包含内核核心函数定义
#include <asm/segment.h>   // 包含段操作相关函数定义

/*
 * 复制inode信息到用户空间的stat结构体
 * @inode: 指向inode结构体的指针，包含文件的元数据
 * @statbuf: 用户空间中的stat结构体指针，用于存储文件状态信息
 * 功能：将inode中的文件状态信息复制到用户空间的stat结构体中
 */
static void cp_stat(struct m_inode *inode, struct stat *statbuf)
{
    struct stat tmp;  // 临时stat结构体，用于在内核空间组装数据
    int i;            // 循环计数器

    // 验证用户空间statbuf缓冲区是否可写，确保访问安全
    verify_area(statbuf, sizeof(*statbuf));
    
    // 从inode中复制文件状态信息到临时结构体tmp
    tmp.st_dev = inode->i_dev;      // 文件所在设备号
    tmp.st_ino = inode->i_num;      // inode节点号
    tmp.st_mode = inode->i_mode;    // 文件类型和权限
    tmp.st_nlink = inode->i_nlinks; // 硬链接数量
    tmp.st_uid = inode->i_uid;      // 文件所有者用户ID
    tmp.st_gid = inode->i_gid;      // 文件所有者组ID
    tmp.st_rdev = inode->i_zone[0]; // 特殊文件的设备号（如设备文件）
    tmp.st_size = inode->i_size;    // 文件大小（字节数）
    tmp.st_atime = inode->i_atime;  // 最后访问时间
    tmp.st_mtime = inode->i_mtime;  // 最后修改时间
    tmp.st_ctime = inode->i_ctime;  // 最后状态改变时间
    
    // 将临时结构体tmp的数据逐个字节复制到用户空间的statbuf
    // 由于内核空间和用户空间分离，需要使用put_fs_byte函数进行安全复制
    for (i = 0; i < sizeof(tmp); i++)
        put_fs_byte(((char *)&tmp)[i], &((char *)statbuf)[i]);
}

/*
 * sys_stat系统调用：通过文件名获取文件状态
 * @filename: 用户空间的文件名指针
 * @statbuf: 用户空间的stat结构体指针，用于存储结果
 * 功能：根据文件名查找对应的inode，并将其状态信息复制到用户空间
 * 返回值：成功返回0，失败返回错误码
 */
int sys_stat(char *filename, struct stat *statbuf)
{
    struct m_inode *inode;  // 用于存储找到的inode

    // 通过文件名查找对应的inode，namei函数会解析路径并返回inode
    if (!(inode = namei(filename)))
        return -ENOENT;  // 如果找不到文件，返回文件不存在错误
    
    // 将inode中的信息复制到用户空间的statbuf
    cp_stat(inode, statbuf);
    
    // 释放inode的引用，减少其引用计数
    iput(inode);
    
    return 0;  // 成功返回0
}

/*
 * sys_fstat系统调用：通过文件描述符获取文件状态
 * @fd: 文件描述符
 * @statbuf: 用户空间的stat结构体指针，用于存储结果
 * 功能：根据文件描述符找到对应的inode，并将其状态信息复制到用户空间
 * 返回值：成功返回0，失败返回错误码
 */
int sys_fstat(unsigned int fd, struct stat *statbuf)
{
    struct file *f;         // 文件结构体指针
    struct m_inode *inode;  // inode结构体指针

    // 检查文件描述符的有效性：
    // 1. 文件描述符不能超过最大打开文件数NR_OPEN
    // 2. 该描述符对应的文件结构体必须存在
    // 3. 文件结构体必须关联有效的inode
    if (fd >= NR_OPEN || !(f = current->filp[fd]) || !(inode = f->f_inode))
        return -EBADF;  // 如果无效，返回错误的文件描述符错误
    
    // 将inode中的信息复制到用户空间的statbuf
    cp_stat(inode, statbuf);
    
    return 0;  // 成功返回0
}