/*
 *  linux/fs/file_dev.c
 *
 *  (C) 1991  Linus Torvalds
 *
 *  功能说明：Linux 0.11 内核中块设备文件的核心读写实现
 *  作用：连接文件系统索引节点（inode）、文件描述符（file）与底层块设备，
 *        实现用户空间与块设备（如硬盘）之间的数据传输，是“一切皆文件”思想的关键实现
 */
#include <errno.h>
#include <fcntl.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

// 通用宏定义：取两个值中的较小值
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
// 通用宏定义：取两个值中的较大值（本文件暂未使用，为扩展预留）
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
/**
 * @brief 从块设备文件读取数据到用户空间缓冲区
 * @param inode 指向文件的索引节点（存储文件元数据：设备号、文件大小等）
 * @param filp 指向文件描述符（存储当前读写位置、打开标志等）
 * @param buf 用户空间缓冲区地址（接收读取的数据）
 * @param count 期望读取的字节数
 * @return 成功：实际读取的字节数；失败：-ERROR（错误码）
 */
int file_read(struct m_inode *inode, struct file *filp, char *buf, int count)
{
    int left;               // 剩余未读取的字节数
    int chars;              // 当前块中实际读取的字节数
    int nr;                 // 物理块号 / 块内偏移量（复用变量）
    struct buffer_head *bh; // 块缓冲区指针（内核与块设备的缓存中间层）
    // 检查输入参数：若期望读取字节数≤0，直接返回0（无数据可读）
    if ((left = count) <= 0)
        return 0;
    // 循环读取数据，直到剩余字节数为0或读取失败
    while (left)
    {
        // 1. 计算当前读写位置对应的物理块号
        // bmap()：根据inode和“逻辑块号”（filp->f_pos / BLOCK_SIZE），转换为设备上的物理块号
        nr = bmap(inode, (filp->f_pos) / BLOCK_SIZE);
        if (nr)
        {
            // 2. 从块设备读取指定物理块到缓冲区
            // bread()：读取inode->i_dev设备上的nr号块，返回缓存区指针；失败则bh为NULL
            bh = bread(inode->i_dev, nr);
            if (!bh)
                break; // 读取失败（如硬件错误），退出循环
        }
        else
        {
            // 无对应物理块（如文件洞：文件中逻辑存在但物理未分配的区域），缓冲区置空
            bh = NULL;
        }
        // 3. 计算当前读写位置在块内的偏移量（块大小为BLOCK_SIZE，通常为1024字节）
        nr = filp->f_pos % BLOCK_SIZE;
        // 4. 确定当前块可读取的最大字节数（避免超出块边界或剩余未读字节数）
        chars = MIN(BLOCK_SIZE - nr, left);
        // 5. 更新文件读写位置和剩余未读字节数
        filp->f_pos += chars; // 读写位置后移“当前读取字节数”
        left -= chars;        // 剩余未读字节数减少“当前读取字节数”
        // 6. 将缓冲区数据复制到用户空间（或文件洞填0）
        if (bh)
        {
            char *p = nr + bh->b_data; // 指向块内偏移位置的数据
            // 逐字节复制：从内核缓冲区（bh->b_data）到用户缓冲区（buf）
            while (chars-- > 0)
                put_fs_byte(*(p++), buf++); // put_fs_byte：跨段写入（内核→用户空间）
            brelse(bh);                     // 释放块缓冲区（将缓存放回空闲链表，避免内存泄漏）
        }
        else
        {
            // 文件洞处理：无物理块，向用户缓冲区填充0
            while (chars-- > 0)
                put_fs_byte(0, buf++);
        }
    }

    // 更新文件的访问时间（inode元数据更新）
    inode->i_atime = CURRENT_TIME;
    // 返回结果：若实际读取字节数>0则返回该值，否则返回错误码
    return (count - left) ? (count - left) : -ERROR;
}
/**
 * @brief 从用户空间缓冲区向块设备文件写入数据
 * @param inode 指向文件的索引节点（存储文件元数据：设备号、文件大小等）
 * @param filp 指向文件描述符（存储当前读写位置、打开标志等）
 * @param buf 用户空间缓冲区地址（提供待写入的数据）
 * @param count 期望写入的字节数
 * @return 成功：实际写入的字节数；失败：-1
 */
int file_write(struct m_inode *inode, struct file *filp, char *buf, int count)
{
    off_t pos;              // 当前写入位置（支持大文件偏移，off_t为长整型）
    int block;              // 物理块号
    int c;                  // 当前块内偏移量 / 实际写入字节数（复用变量）
    struct buffer_head *bh; // 块缓冲区指针
    char *p;                // 指向块缓冲区内的写入位置
    int i = 0;              // 已写入的总字节数
    /* 注：多进程同时追加写（O_APPEND）可能存在竞争问题，
     * 但Linux 0.11为简化设计未处理该场景，实际使用中需注意
     */
    // 确定写入起始位置：O_APPEND标志则从文件末尾开始，否则从当前读写位置开始
    if (filp->f_flags & O_APPEND)
        pos = inode->i_size;
    else
        pos = filp->f_pos;

    // 循环写入数据，直到写入完所有字节或失败
    while (i < count)
    {
        // 1. 为当前逻辑块分配物理块（若不存在则创建）
        // create_block()：根据inode和逻辑块号（pos/BLOCK_SIZE），分配物理块并更新inode
        block = create_block(inode, pos / BLOCK_SIZE);
        if (!block)
            break; // 分配物理块失败（如磁盘满），退出循环
        // 2. 读取该物理块到缓冲区（若缓存未命中则从设备读取）
        bh = bread(inode->i_dev, block);
        if (!bh)
            break; // 读取缓冲区失败，退出循环
        // 3. 计算当前写入位置在块内的偏移量
        c = pos % BLOCK_SIZE;
        // 4. 准备写入：指向块缓冲区内的偏移位置，标记缓冲区为“脏”（需写回设备）
        p = c + bh->b_data;
        bh->b_dirt = 1; // 脏标记：表示缓冲区数据已修改，需后续写回块设备
        // 5. 确定当前块可写入的最大字节数
        c = BLOCK_SIZE - c; // 块内剩余可写入空间
        if (c > count - i)  // 若剩余空间大于待写入字节数，取待写入字节数
            c = count - i;
        // 6. 更新写入位置和文件大小（若写入超出原文件大小）
        pos += c;
        if (pos > inode->i_size)
        {
            inode->i_size = pos; // 更新文件大小（inode元数据）
            inode->i_dirt = 1;   // 标记inode为“脏”，需写回磁盘
        }
        // 7. 更新已写入总字节数
        i += c;
        // 8. 从用户空间缓冲区复制数据到内核块缓冲区
        while (c-- > 0)
            *(p++) = get_fs_byte(buf++); // get_fs_byte：跨段读取（用户空间→内核）
        // 9. 释放块缓冲区（放回空闲链表）
        brelse(bh);
    }
    // 更新文件的修改时间（inode元数据）
    inode->i_mtime = CURRENT_TIME;
    // 若不是追加写模式，更新文件描述符的当前读写位置和inode的改变时间
    if (!(filp->f_flags & O_APPEND))
    {
        filp->f_pos = pos;
        inode->i_ctime = CURRENT_TIME; // ctime：inode元数据改变时间
    }
    // 返回结果：若实际写入字节数>0则返回该值，否则返回-1（失败）
    return (i ? i : -1);
}