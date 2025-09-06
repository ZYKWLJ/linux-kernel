/*
 *  linux/fs/truncate.c
 *
 *  (C) 1991  Linus Torvalds
 *  这是Linux内核中的文件截断功能实现
 */

#include <linux/sched.h> // 包含进程调度相关定义
#include <sys/stat.h>    // 包含文件状态相关宏定义

/*
 * 释放间接块
 * @dev: 设备号
 * @block: 间接块号
 * 功能：释放一个间接块及其所指向的所有数据块
 */
static void free_ind(int dev, int block)
{
    struct buffer_head *bh; // 缓冲区头指针，用于操作磁盘块
    unsigned short *p;      // 用于遍历块中的数据（块号）
    int i;                  // 循环计数器

    if (!block) // 如果块号为0，说明没有该间接块，直接返回
        return;

    // 读取指定设备上的指定块到缓冲区，返回缓冲区头指针
    if ((bh = bread(dev, block)))
    {
        p = (unsigned short *)bh->b_data; // 获取缓冲区中的数据部分（存储块号的数组）

        // 遍历间接块中的所有512个块号项（每个块号占2字节，512*2=1024字节，正好是一个块大小）
        for (i = 0; i < 512; i++, p++)
            if (*p)                  // 如果块号不为0，说明指向了一个数据块
                free_block(dev, *p); // 释放该数据块

        brelse(bh); // 释放缓冲区（解除占用并标记为脏）
    }

    free_block(dev, block); // 释放间接块本身
}

/*
 * 释放双重间接块
 * @dev: 设备号
 * @block: 双重间接块号
 * 功能：释放一个双重间接块及其所指向的所有间接块和数据块
 */
static void free_dind(int dev, int block)
{
    struct buffer_head *bh; // 缓冲区头指针
    unsigned short *p;      // 用于遍历块中的数据（块号）
    int i;                  // 循环计数器

    if (!block) // 如果块号为0，直接返回
        return;

    // 读取双重间接块到缓冲区
    if ((bh = bread(dev, block)))
    {
        p = (unsigned short *)bh->b_data; // 获取缓冲区中的数据部分

        // 遍历双重间接块中的所有512个块号项
        for (i = 0; i < 512; i++, p++)
            if (*p)                // 如果块号不为0，说明指向了一个间接块
                free_ind(dev, *p); // 释放该间接块（会递归释放其指向的数据块）

        brelse(bh); // 释放缓冲区
    }

    free_block(dev, block); // 释放双重间接块本身
}

/*
 * 截断文件
 * @inode: 要截断的文件的inode结构指针
 * 功能：将文件大小设为0，并释放所有关联的数据块
 */
void truncate(struct m_inode *inode)
{
    int i; // 循环计数器

    // 检查文件类型，只有普通文件或目录文件才能被截断
    if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
        return;

    // 释放直接块（前7个块是直接块）
    for (i = 0; i < 7; i++)
        if (inode->i_zone[i]) // 如果块号不为0
        {
            free_block(inode->i_dev, inode->i_zone[i]); // 释放该直接块
            inode->i_zone[i] = 0;                       // 清空块号
        }

    // 释放间接块（第8个块是间接块指针）
    free_ind(inode->i_dev, inode->i_zone[7]);
    // 释放双重间接块（第9个块是双重间接块指针）
    free_dind(inode->i_dev, inode->i_zone[8]);

    // 清空间接块和双重间接块的指针
    inode->i_zone[7] = inode->i_zone[8] = 0;
    // 将文件大小设置为0
    inode->i_size = 0;
    // 标记inode为脏，需要写回磁盘
    inode->i_dirt = 1;
    // 更新文件的修改时间和状态改变时间为当前时间
    inode->i_mtime = inode->i_ctime = CURRENT_TIME;
}