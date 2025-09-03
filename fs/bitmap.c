/*
 *  linux/fs/bitmap.c
 *
 *  (C) 1991  Linus Torvalds
 */

/* bitmap.c 包含处理inode和块位图的代码 */
#include <string.h>

#include <linux/sched.h>
#include <linux/kernel.h>

/* 清除一个内存块（设置为0） */
#define clear_block(addr)                  \
    __asm__ __volatile__("cld\n\t"         \  // 清除方向标志（向前移动）
                         "rep\n\t"         \  // 重复执行
                         "stosl" ::"a"(0), \  // 将EAX(0)存储到EDI指向的位置
                         "c"(BLOCK_SIZE / 4), "D"((long)(addr))) // 计数=块大小/4，目标地址=addr

/* 设置指定位 */
#define set_bit(nr, addr) ({\
register int res ; \
__asm__ __volatile__("btsl %2,%3\n\tsetb %%al": \  // 位测试并设置，然后根据CF设置AL
"=a" (res):"0" (0),"r" (nr),"m" (*(addr))); \
res; })

/* 清除指定位 */
#define clear_bit(nr, addr) ({\
register int res ; \
__asm__ __volatile__("btrl %2,%3\n\tsetnb %%al": \  // 位测试并复位，然后根据CF设置AL
"=a" (res):"0" (0),"r" (nr),"m" (*(addr))); \
res; })

/* 查找第一个零位（空闲位） */
#define find_first_zero(addr) ({ \
int __res; \
__asm__ __volatile__ ("cld\n" \          // 清除方向标志
	"1:\tlodsl\n\t" \                   // 加载ESI到EAX，ESI增加4
	"notl %%eax\n\t" \                  // 按位取反（0变1，1变0）
	"bsfl %%eax,%%edx\n\t" \            // 从低位扫描，查找第一个设置位
	"je 2f\n\t" \                       // 如果没找到（全0），跳转到标签2
	"addl %%edx,%%ecx\n\t" \            // 将找到的位偏移加到ECX
	"jmp 3f\n" \                        // 跳转到标签3（结束）
	"2:\taddl $32,%%ecx\n\t" \          // 增加32位（检查下一个字）
	"cmpl $8192,%%ecx\n\t" \            // 比较是否达到8192（位图大小）
	"jl 1b\n" \                         // 如果小于，继续循环
	"3:" \                              // 结束标签
	:"=c" (__res):"c" (0),"S" (addr)); \  // 输出：ECX到__res，输入：ECX=0，ESI=addr
__res; })

/* 释放数据块 */
void free_block(int dev, int block)
{
    struct super_block *sb;
    struct buffer_head *bh;

    // 获取设备超级块
    if (!(sb = get_super(dev)))
        panic("trying to free block on nonexistent device");
    // 检查块号是否在数据区范围内
    if (block < sb->s_firstdatazone || block >= sb->s_nzones)
        panic("trying to free block not in datazone");
    // 获取块的缓冲头
    bh = get_hash_table(dev, block);
    if (bh)
    {
        // 检查引用计数
        if (bh->b_count != 1)
        {
            printk("trying to free block (%04x:%d), count=%d\n",
                   dev, block, bh->b_count);
            return;
        }
        // 清除脏位和更新标志
        bh->b_dirt = 0;
        bh->b_uptodate = 0;
        // 释放缓冲区
        brelse(bh);
    }
    // 计算位图中的位置（减去数据区起始偏移）
    block -= sb->s_firstdatazone - 1;
    // 清除位图中的位（标记为空闲）
    if (clear_bit(block & 8191, sb->s_zmap[block / 8192]->b_data))
    {
        // 如果位已经是清除状态，报错
        printk("block (%04x:%d) ", dev, block + sb->s_firstdatazone - 1);
        panic("free_block: bit already cleared");
    }
    // 标记位图缓冲区为脏（需要写回磁盘）
    sb->s_zmap[block / 8192]->b_dirt = 1;
}

/* 分配新数据块 */
int new_block(int dev)
{
    struct buffer_head *bh;
    struct super_block *sb;
    int i, j;

    // 获取设备超级块
    if (!(sb = get_super(dev)))
        panic("trying to get new block from nonexistant device");
    j = 8192; // 初始化为最大位图索引
    // 遍历所有块位图（共8个，每个管理8192个块）
    for (i = 0; i < 8; i++)
        if ((bh = sb->s_zmap[i]))
            // 查找第一个空闲位
            if ((j = find_first_zero(bh->b_data)) < 8192)
                break;
    // 检查是否找到空闲块
    if (i >= 8 || !bh || j >= 8192)
        return 0;
    // 设置位图中的位（标记为已使用）
    if (set_bit(j, bh->b_data))
        panic("new_block: bit already set");
    // 标记位图缓冲区为脏
    bh->b_dirt = 1;
    // 计算实际块号（加上数据区起始偏移）
    j += i * 8192 + sb->s_firstdatazone - 1;
    // 检查块号是否有效
    if (j >= sb->s_nzones)
        return 0;
    // 获取新块的缓冲区
    if (!(bh = getblk(dev, j)))
        panic("new_block: cannot get block");
    // 检查引用计数
    if (bh->b_count != 1)
        panic("new block: count is != 1");
    // 清空块数据（填充0）
    clear_block(bh->b_data);
    // 设置更新标志和脏标志
    bh->b_uptodate = 1;
    bh->b_dirt = 1;
    // 释放缓冲区（但块仍被分配）
    brelse(bh);
    return j; // 返回分配的块号
}

/* 释放inode */
void free_inode(struct m_inode *inode)
{
    struct super_block *sb;
    struct buffer_head *bh;

    // 检查inode是否有效
    if (!inode)
        return;
    // 检查设备号是否有效
    if (!inode->i_dev)
    {
        memset(inode, 0, sizeof(*inode));
        return;
    }
    // 检查引用计数
    if (inode->i_count > 1)
    {
        printk("trying to free inode with count=%d\n", inode->i_count);
        panic("free_inode");
    }
    // 检查链接数（应为0才能释放）
    if (inode->i_nlinks)
        panic("trying to free inode with links");
    // 获取设备超级块
    if (!(sb = get_super(inode->i_dev)))
        panic("trying to free inode on nonexistent device");
    // 检查inode号是否有效
    if (inode->i_num < 1 || inode->i_num > sb->s_ninodes)
        panic("trying to free inode 0 or nonexistant inode");
    // 获取inode位图（每个位图管理8192个inode）
    if (!(bh = sb->s_imap[inode->i_num >> 13]))
        panic("nonexistent imap in superblock");
    // 清除位图中的位（标记为空闲）
    if (clear_bit(inode->i_num & 8191, bh->b_data))
        printk("free_inode: bit already cleared.\n\r");
    // 标记位图缓冲区为脏
    bh->b_dirt = 1;
    // 清空inode结构
    memset(inode, 0, sizeof(*inode));
}

/* 分配新inode */
struct m_inode *new_inode(int dev)
{
    struct m_inode *inode;
    struct super_block *sb;
    struct buffer_head *bh;
    int i, j;

    // 获取空闲inode结构
    if (!(inode = get_empty_inode()))
        return NULL;
    // 获取设备超级块
    if (!(sb = get_super(dev)))
        panic("new_inode with unknown device");
    j = 8192; // 初始化为最大位图索引
    // 遍历所有inode位图（共8个，每个管理8192个inode）
    for (i = 0; i < 8; i++)
        if ((bh = sb->s_imap[i]))
            // 查找第一个空闲位
            if ((j = find_first_zero(bh->b_data)) < 8192)
                break;
    // 检查是否找到空闲inode
    if (!bh || j >= 8192 || j + i * 8192 > sb->s_ninodes)
    {
        iput(inode); // 释放inode结构
        return NULL;
    }
    // 设置位图中的位（标记为已使用）
    if (set_bit(j, bh->b_data))
        panic("new_inode: bit already set");
    // 标记位图缓冲区为脏
    bh->b_dirt = 1;
    // 设置inode属性
    inode->i_count = 1;           // 引用计数
    inode->i_nlinks = 1;          // 链接数
    inode->i_dev = dev;           // 设备号
    inode->i_uid = current->euid; // 用户ID
    inode->i_gid = current->egid; // 组ID
    inode->i_dirt = 1;            // 脏标志
    inode->i_num = j + i * 8192;  // 计算inode号
    inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME; // 设置时间
    return inode; // 返回新inode
}