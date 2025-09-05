/*
 *  linux/fs/block_dev.c
 *
 *  (C) 1991  Linus Torvalds
 *  块设备读写操作的核心实现文件
 *  负责处理块设备（如硬盘、软盘等）的读写请求
 */

#include <errno.h>        // 包含错误码定义
#include <linux/sched.h>  // 包含进程调度相关定义
#include <linux/kernel.h> // 包含内核核心函数和宏定义
#include <asm/segment.h>  // 包含段操作相关函数（用户空间与内核空间数据传输）
#include <asm/system.h>   // 包含系统相关操作宏（如中断控制）

/**
 * 向块设备写入数据
 * @param dev 设备号，标识要操作的块设备
 * @param pos 指向文件当前读写位置的指针（将被更新）
 * @param buf 用户空间中的数据缓冲区，存放要写入的数据
 * @param count 要写入的字节数
 * @return 成功写入的字节数，失败返回错误码（-EIO）
 */
int block_write(int dev, long *pos, char *buf, int count)
{
    int block = *pos >> BLOCK_SIZE_BITS;  // 计算当前位置所在的块号
                                          // BLOCK_SIZE_BITS是块大小的对数（如10表示块大小1024字节）
    int offset = *pos & (BLOCK_SIZE - 1); // 计算在块内的偏移量（BLOCK_SIZE是块大小，如1024）
    int chars;                            // 本次要处理的字节数
    int written = 0;                      // 已写入的总字节数
    struct buffer_head *bh;               // 缓冲区头指针，用于管理磁盘块缓存
    register char *p;                     // 指向缓冲区中实际数据的指针（寄存器变量，速度更快）

    // 循环处理所有要写入的数据，直到count减为0
    while (count > 0)
    {
        // 计算当前块中可写入的字节数（从偏移量到块末尾）
        chars = BLOCK_SIZE - offset;
        // 如果剩余要写入的字节数小于当前块可写空间，则只写入剩余字节
        if (chars > count)
            chars = count;

        // 获取缓冲区：如果要写满一整块，直接分配新块；否则预读后续块提升性能
        if (chars == BLOCK_SIZE)
            bh = getblk(dev, block); // 获取指定设备和块号的缓冲区（不读取数据）
        else
            // 读取当前块并预读后续2个块（-1表示结束），提升连续读写性能
            bh = breada(dev, block, block + 1, block + 2, -1);

        block++; // 准备处理下一个块

        // 如果获取缓冲区失败，返回已写入的字节数（如果有）或错误码
        if (!bh)
            return written ? written : -EIO;

        // 设置数据指针：缓冲区数据起始位置 + 偏移量
        p = offset + bh->b_data;
        offset = 0; // 下一次将从新块的开头开始写入，所以偏移量重置为0

        *pos += chars;    // 更新文件位置
        written += chars; // 更新已写入字节数
        count -= chars;   // 更新剩余要写入的字节数

        // 将用户空间缓冲区的数据复制到内核缓冲区
        while (chars-- > 0)
            *(p++) = get_fs_byte(buf++); // get_fs_byte从用户空间读取一个字节

        bh->b_dirt = 1; // 标记缓冲区为"脏"（数据已修改，需要写回磁盘）
        brelse(bh);     // 释放缓冲区（将其放回空闲链表，可能触发写回）
    }
    return written; // 返回成功写入的总字节数
}

/**
 * 从块设备读取数据
 * @param dev 设备号，标识要操作的块设备
 * @param pos 指向文件当前读写位置的指针（将被更新）
 * @param buf 用户空间中的数据缓冲区，用于存放读取的数据
 * @param count 要读取的字节数
 * @return 成功读取的字节数，失败返回错误码（-EIO）
 */
int block_read(int dev, unsigned long *pos, char *buf, int count)
{
    int block = *pos >> BLOCK_SIZE_BITS;  // 计算当前位置所在的块号
    int offset = *pos & (BLOCK_SIZE - 1); // 计算在块内的偏移量
    int chars;                            // 本次要处理的字节数
    int read = 0;                         // 已读取的总字节数
    struct buffer_head *bh;               // 缓冲区头指针
    register char *p;                     // 指向缓冲区中实际数据的指针

    // 循环处理所有要读取的数据，直到count减为0
    while (count > 0)
    {
        // 计算当前块中可读取的字节数（从偏移量到块末尾）
        chars = BLOCK_SIZE - offset;
        // 如果剩余要读取的字节数小于当前块可读取空间，则只读取剩余字节
        if (chars > count)
            chars = count;

        // 读取当前块并预读后续2个块，提升连续读取性能
        if (!(bh = breada(dev, block, block + 1, block + 2, -1)))
            return read ? read : -EIO; // 读取失败，返回已读取字节数或错误码

        block++; // 准备处理下一个块

        // 设置数据指针：缓冲区数据起始位置 + 偏移量
        p = offset + bh->b_data;
        offset = 0; // 下一次将从新块的开头开始读取，偏移量重置为0

        *pos += chars;  // 更新文件位置
        read += chars;  // 更新已读取字节数
        count -= chars; // 更新剩余要读取的字节数

        // 将内核缓冲区的数据复制到用户空间缓冲区
        while (chars-- > 0)
            put_fs_byte(*(p++), buf++); // put_fs_byte向用户空间写入一个字节

        brelse(bh); // 释放缓冲区
    }
    return read; // 返回成功读取的总字节数
}