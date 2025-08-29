/*
 *  linux/kernel/blk_drv/ramdisk.c
 *
 *  由Theodore Ts'o编写，1991年12月2日
 *  内存磁盘(ramdisk)驱动：将一部分内存模拟为磁盘设备
 */

#include <string.h> // 字符串和内存操作函数声明

#include <linux/config.h> // 内核配置选项
#include <linux/sched.h>  // 进程调度相关定义
#include <linux/fs.h>     // 文件系统相关定义
#include <linux/kernel.h> // 内核核心函数和宏
#include <asm/system.h>   // 汇编相关系统函数
#include <asm/segment.h>  // 内存段相关定义
#include <asm/memory.h>   // 内存管理相关定义

#define MAJOR_NR 1 // ramdisk的主设备号是1
#include "blk.h"   // 块设备相关定义

char *rd_start;    // 指向ramdisk在内存中的起始地址
int rd_length = 0; // ramdisk的总长度(字节)

/*
 * 处理ramdisk的请求
 * 响应块设备层发出的读写请求
 */
void do_rd_request(void)
{
    int len;    // 本次请求的长度
    char *addr; // 指向ramdisk中对应的内存地址

    INIT_REQUEST; // 初始化请求处理，检查当前请求的有效性
    // 计算ramdisk中的起始地址：每个扇区512字节(左移9位相当于乘以512)
    addr = rd_start + (CURRENT->sector << 9);
    // 计算请求的总长度：扇区数 × 512字节
    len = CURRENT->nr_sectors << 9;

    // 检查设备号是否有效，以及请求的地址范围是否超出ramdisk的范围
    if ((MINOR(CURRENT->dev) != 1) || (addr + len > rd_start + rd_length))
    {
        end_request(0); // 处理失败，通知块设备层
        goto repeat;    // 继续处理下一个请求
    }

    // 根据请求类型执行相应操作
    if (CURRENT->cmd == WRITE)
    {
        // 写操作：将缓冲区数据复制到ramdisk
        (void)memcpy(addr, CURRENT->buffer, len);
    }
    else if (CURRENT->cmd == READ)
    {
        // 读操作：将ramdisk数据复制到缓冲区
        (void)memcpy(CURRENT->buffer, addr, len);
    }
    else
        // 未知命令，触发内核恐慌
        panic("unknown ramdisk-command");

    end_request(1); // 处理成功，通知块设备层
    goto repeat;    // 继续处理下一个请求
}

/*
 * 初始化ramdisk
 * 返回需要保留的内存大小
 * mem_start: ramdisk的起始内存地址
 * length:    ramdisk的长度
 */
long rd_init(long mem_start, int length)
{
    int i;
    char *cp;

    // 注册ramdisk的请求处理函数
    blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
    rd_start = (char *)mem_start; // 设置ramdisk的起始地址
    rd_length = length;           // 设置ramdisk的长度

    // 将ramdisk的所有字节初始化为0
    cp = rd_start;
    for (i = 0; i < length; i++)
        *cp++ = '\0';

    return (length); // 返回ramdisk占用的内存大小
}

/*
 * 如果根设备是ramdisk，尝试加载它
 * 为了实现这一点，根设备最初被设置为软盘，
 * 之后我们将其改为ramdisk
 */
void rd_load(void)
{
    struct buffer_head *bh; // 缓冲区头指针
    struct super_block s;   // 超级块结构
    int block = 256;        // 从块256开始读取(软盘上的ramdisk镜像位置)
    int i = 1;
    int nblocks; // 总块数
    char *cp;    // 移动指针，用于复制数据

    // 如果ramdisk长度为0，直接返回(未启用ramdisk)
    if (!rd_length)
        return;

    // 打印ramdisk信息
    printk("Ram disk: %d bytes, starting at 0x%x\n", rd_length,
           (int)rd_start);

    // 如果当前根设备不是软盘(主设备号2)，直接返回
    if (MAJOR(ROOT_DEV) != 2)
        return;

    // 预读几个块：block+1, block, block+2，提高加载速度
    bh = breada(ROOT_DEV, block + 1, block, block + 2, -1);
    if (!bh)
    {
        printk("Disk error while looking for ramdisk!\n");
        return;
    }

    // 从缓冲区中读取超级块信息
    *((struct d_super_block *)&s) = *((struct d_super_block *)bh->b_data);
    brelse(bh); // 释放缓冲区

    // 检查超级块的魔数，验证是否为有效的文件系统
    if (s.s_magic != SUPER_MAGIC)
    {
        // 没有找到ramdisk镜像，使用正常的软盘启动
        return;
    }

    // 计算总块数：区数 × 每个区的块数(基于区大小的对数)
    nblocks = s.s_nzones << s.s_log_zone_size;

    // 检查ramdisk是否有足够的空间容纳镜像
    if (nblocks > (rd_length >> BLOCK_SIZE_BITS))
    {
        printk("Ram disk image too big!  (%d blocks, %d avail)\n",
               nblocks, rd_length >> BLOCK_SIZE_BITS);
        return;
    }

    // 开始加载ramdisk镜像
    printk("Loading %d bytes into ram disk... 0000k",
           nblocks << BLOCK_SIZE_BITS);
    cp = rd_start; // 指向ramdisk的起始地址

    // 循环读取所有块
    while (nblocks)
    {
        // 如果剩余块数大于2，使用预读功能提高效率
        if (nblocks > 2)
            bh = breada(ROOT_DEV, block, block + 1, block + 2, -1);
        else
            bh = bread(ROOT_DEV, block); // 读取单个块

        if (!bh)
        {
            printk("I/O error on block %d, aborting load\n", block);
            return;
        }

        // 将读取的数据复制到ramdisk
        (void)memcpy(cp, bh->b_data, BLOCK_SIZE);
        brelse(bh); // 释放缓冲区

        // 更新进度显示
        printk("\010\010\010\010\010%4dk", i);
        cp += BLOCK_SIZE; // 移动ramdisk中的指针
        block++;          // 下一个块
        nblocks--;        // 剩余块数减1
        i++;
    }

    // 加载完成
    printk("\010\010\010\010\010done \n");
    ROOT_DEV = 0x0101; // 将根设备改为ramdisk(主设备号1，次设备号1)
}
