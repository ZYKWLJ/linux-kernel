/*
 * linux/fs/buffer.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * 'buffer.c' 实现了缓冲区缓存功能。
 * 通过 NEVER 让中断改变缓冲区（除了数据本身）来避免竞态条件，而是让调用者来做。
 * 注意！由于中断可以唤醒调用者，需要一些 cli-sti 序列来检查调用导致的睡眠。
 * 这些操作应该非常快速（我希望如此）。
 */

/*
 * 注意！这里有一个不协调的地方：检查软盘是否更换。
 * 我认为这是最适合的地方，因为它应该使已更改的软盘缓存失效。
 */

#include <stdarg.h>

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/io.h>

// 外部变量声明
extern int end;                 // 内核结束地址（由链接器设置）
extern void put_super(int);     // 释放超级块
extern void invalidate_inodes(int); // 使inode失效

// 全局变量定义
struct buffer_head *start_buffer = (struct buffer_head *)&end; // 缓冲区起始地址
struct buffer_head *hash_table[NR_HASH];      // 哈希表，用于快速查找缓冲区
static struct buffer_head *free_list;         // 空闲缓冲区链表
static struct task_struct *buffer_wait = NULL; // 等待缓冲区的进程队列
int NR_BUFFERS = 0;                           // 缓冲区数量

// 等待缓冲区解锁（内联函数）
static inline void wait_on_buffer(struct buffer_head *bh)
{
    cli();                     // 关中断
    while (bh->b_lock)         // 如果缓冲区被锁定
        sleep_on(&bh->b_wait); // 睡眠等待
    sti();                     // 开中断
}

// 同步所有缓冲区到磁盘
int sys_sync(void)
{
    int i;
    struct buffer_head *bh;

    sync_inodes(); /* 将inode写入缓冲区 */
    bh = start_buffer;
    for (i = 0; i < NR_BUFFERS; i++, bh++) // 遍历所有缓冲区
    {
        wait_on_buffer(bh);      // 等待缓冲区解锁
        if (bh->b_dirt)          // 如果缓冲区脏（已修改）
            ll_rw_block(WRITE, bh); // 写入磁盘
    }
    return 0;
}

// 同步指定设备的所有缓冲区到磁盘
int sync_dev(int dev)
{
    int i;
    struct buffer_head *bh;

    bh = start_buffer;
    for (i = 0; i < NR_BUFFERS; i++, bh++) // 第一次遍历
    {
        if (bh->b_dev != dev)    // 只处理指定设备的缓冲区
            continue;
        wait_on_buffer(bh);
        if (bh->b_dev == dev && bh->b_dirt)
            ll_rw_block(WRITE, bh);
    }
    sync_inodes();               // 同步inode
    bh = start_buffer;
    for (i = 0; i < NR_BUFFERS; i++, bh++) // 第二次遍历
    {
        if (bh->b_dev != dev)
            continue;
        wait_on_buffer(bh);
        if (bh->b_dev == dev && bh->b_dirt)
            ll_rw_block(WRITE, bh);
    }
    return 0;
}

// 使指定设备的所有缓冲区失效（内联函数）
static void inline invalidate_buffers(int dev)
{
    int i;
    struct buffer_head *bh;

    bh = start_buffer;
    for (i = 0; i < NR_BUFFERS; i++, bh++)
    {
        if (bh->b_dev != dev)    // 只处理指定设备的缓冲区
            continue;
        wait_on_buffer(bh);      // 等待缓冲区解锁
        if (bh->b_dev == dev)
            bh->b_uptodate = bh->b_dirt = 0; // 标记为非最新和未修改
    }
}

/*
 * 此例程检查软盘是否已更改，如果是，则使所有相关的缓冲区缓存项失效。
 * 这是一个相对较慢的例程，因此我们应尽量减少使用它。
 * 因此它只在'mount'或'open'时被调用。
 * 我认为这是结合速度和效用的最佳方式。
 * 在操作过程中更换磁盘的人应该承受数据丢失的后果 :-)
 *
 * 注意！虽然目前这仅适用于软盘，但思想是任何额外的可移动块设备都将使用此例程，
 * 并且mount/open不需要知道软盘/任何东西是特殊的。
 */
void check_disk_change(int dev)
{
    int i;

    if (MAJOR(dev) != 2)        // 主设备号2是软盘
        return;
    if (!floppy_change(dev & 0x03)) // 检查软盘是否更换
        return;
    for (i = 0; i < NR_SUPER; i++) // 遍历所有超级块
        if (super_block[i].s_dev == dev)
            put_super(super_block[i].s_dev); // 释放超级块
    invalidate_inodes(dev);      // 使inode失效
    invalidate_buffers(dev);     // 使缓冲区失效
}

// 哈希函数宏定义
#define _hashfn(dev, block) (((unsigned)(dev ^ block)) % NR_HASH)
#define hash(dev, block) hash_table[_hashfn(dev, block)]

// 从队列中移除缓冲区（内联函数）
static inline void remove_from_queues(struct buffer_head *bh)
{
    /* 从哈希队列中移除 */
    if (bh->b_next)
        bh->b_next->b_prev = bh->b_prev;
    if (bh->b_prev)
        bh->b_prev->b_next = bh->b_next;
    if (hash(bh->b_dev, bh->b_blocknr) == bh)
        hash(bh->b_dev, bh->b_blocknr) = bh->b_next;
    
    /* 从空闲列表中移除 */
    if (!(bh->b_prev_free) || !(bh->b_next_free))
        panic("Free block list corrupted"); // 空闲列表损坏
    bh->b_prev_free->b_next_free = bh->b_next_free;
    bh->b_next_free->b_prev_free = bh->b_prev_free;
    if (free_list == bh)
        free_list = bh->b_next_free;
}

// 将缓冲区插入队列（内联函数）
static inline void insert_into_queues(struct buffer_head *bh)
{
    /* 放到空闲列表末尾 */
    bh->b_next_free = free_list;
    bh->b_prev_free = free_list->b_prev_free;
    free_list->b_prev_free->b_next_free = bh;
    free_list->b_prev_free = bh;
    
    /* 如果缓冲区有设备，将其放入新的哈希队列 */
    bh->b_prev = NULL;
    bh->b_next = NULL;
    if (!bh->b_dev)
        return;
    bh->b_next = hash(bh->b_dev, bh->b_blocknr);
    hash(bh->b_dev, bh->b_blocknr) = bh;
    bh->b_next->b_prev = bh;
}

// 在哈希表中查找指定设备和块号的缓冲区
static struct buffer_head *find_buffer(int dev, int block)
{
    struct buffer_head *tmp;

    // 遍历哈希链表
    for (tmp = hash(dev, block); tmp != NULL; tmp = tmp->b_next)
        if (tmp->b_dev == dev && tmp->b_blocknr == block)
            return tmp;
    return NULL;
}

/*
 * 为什么这样做，我听到你说...原因是竞态条件。
 * 由于我们不锁定缓冲区（除非正在读取），当我们睡眠时可能会发生某些事情
 * （例如读取错误会将其标记为坏）。目前这不应该真正发生，但代码已准备好。
 */
struct buffer_head *get_hash_table(int dev, int block)
{
    struct buffer_head *bh;

    for (;;) // 无限循环，直到找到有效的缓冲区
    {
        if (!(bh = find_buffer(dev, block))) // 查找缓冲区
            return NULL;
        bh->b_count++;           // 增加引用计数
        wait_on_buffer(bh);      // 等待缓冲区解锁
        // 再次检查，因为睡眠期间可能发生了变化
        if (bh->b_dev == dev && bh->b_blocknr == block)
            return bh;
        bh->b_count--;           // 如果不是我们要的，减少引用计数
    }
}

/*
 * 这是getblk，它不是很清晰，再次是为了防止竞态条件。
 * 大部分代码很少使用（例如重复），所以它应该比看起来更高效。
 *
 * 算法已更改：希望更好，并移除了一个难以发现的错误。
 */
// 坏度计算宏：脏缓冲区比锁定缓冲区更不适合重用
#define BADNESS(bh) (((bh)->b_dirt << 1) + (bh)->b_lock)
struct buffer_head *getblk(int dev, int block)
{
    struct buffer_head *tmp, *bh;

repeat:
    if ((bh = get_hash_table(dev, block))) // 首先检查是否已在缓存中
        return bh;
    
    // 在空闲列表中寻找最佳缓冲区
    tmp = free_list;
    do
    {
        if (tmp->b_count)        // 如果正在使用，跳过
            continue;
        if (!bh || BADNESS(tmp) < BADNESS(bh)) // 寻找"坏度"最低的缓冲区
        {
            bh = tmp;
            if (!BADNESS(tmp))   // 找到完美的缓冲区（未锁定且未修改）
                break;
        }
        /* 重复直到找到合适的 */
    } while ((tmp = tmp->b_next_free) != free_list);
    
    if (!bh) // 没有找到可用缓冲区
    {
        sleep_on(&buffer_wait);  // 睡眠等待缓冲区
        goto repeat;             // 重试
    }
    
    wait_on_buffer(bh);          // 等待选中的缓冲区解锁
    if (bh->b_count)             // 如果现在被使用了
        goto repeat;             // 重试
    
    while (bh->b_dirt)           // 如果缓冲区脏
    {
        sync_dev(bh->b_dev);     // 同步到磁盘
        wait_on_buffer(bh);      // 等待写入完成
        if (bh->b_count)         // 如果现在被使用了
            goto repeat;         // 重试
    }
    
    /* 注意！！当我们睡眠等待这个块时，其他人可能已经将"这个"块添加到缓存中。检查它 */
    if (find_buffer(dev, block)) // 再次检查是否已存在
        goto repeat;
    
    /* 好的，最终我们知道这个缓冲区是唯一的，并且未使用(b_count=0)，未锁定(b_lock=0)，且干净 */
    bh->b_count = 1;             // 设置引用计数
    bh->b_dirt = 0;              // 清除脏标志
    bh->b_uptodate = 0;          // 清除最新标志
    remove_from_queues(bh);      // 从当前队列移除
    bh->b_dev = dev;             // 设置设备号
    bh->b_blocknr = block;       // 设置块号
    insert_into_queues(bh);      // 插入新队列
    return bh;
}

// 释放缓冲区（减少引用计数，必要时唤醒等待进程）
void brelse(struct buffer_head *buf)
{
    if (!buf)
        return;
    wait_on_buffer(buf);         // 等待缓冲区解锁
    if (!(buf->b_count--))       // 减少引用计数，检查是否已为0
        panic("Trying to free free buffer");
    wake_up(&buffer_wait);       // 唤醒等待缓冲区的进程
}

/*
 * bread() 读取指定块并返回包含它的缓冲区。
 * 如果块不可读，则返回NULL。
 */
struct buffer_head *bread(int dev, int block)
{
    struct buffer_head *bh;

    if (!(bh = getblk(dev, block))) // 获取缓冲区
        panic("bread: getblk returned NULL\n");
    if (bh->b_uptodate)          // 如果数据是最新的
        return bh;
    ll_rw_block(READ, bh);       // 从设备读取数据
    wait_on_buffer(bh);          // 等待读取完成
    if (bh->b_uptodate)          // 如果读取成功
        return bh;
    brelse(bh);                  // 否则释放缓冲区
    return NULL;                 // 返回NULL表示失败
}

// 块复制宏（使用汇编优化）
#define COPYBLK(from, to)                      \
    __asm__("cld\n\t"                          \ // 清除方向标志（向前复制）
            "rep\n\t"                          \ // 重复
            "movsl\n\t" ::"c"(BLOCK_SIZE / 4), \ // 复制次数（块大小/4，因为movsl每次复制4字节）
            "S"(from), "D"(to))                // 源地址，目标地址

/*
 * bread_page 将四个缓冲区读入内存中所需地址。
 * 它是一个独立的函数，因为通过同时读取它们可以获得一些速度，
 * 而不是等待一个被读取，然后再读取另一个等等。
 */
void bread_page(unsigned long address, int dev, int b[4])
{
    struct buffer_head *bh[4];
    int i;

    // 第一阶段：请求所有块的读取
    for (i = 0; i < 4; i++)
        if (b[i]) // 如果有块号
        {
            if ((bh[i] = getblk(dev, b[i]))) // 获取缓冲区
                if (!bh[i]->b_uptodate)      // 如果数据不是最新的
                    ll_rw_block(READ, bh[i]); // 发起读取请求
        }
        else
            bh[i] = NULL;
    
    // 第二阶段：等待读取完成并复制数据
    for (i = 0; i < 4; i++, address += BLOCK_SIZE)
        if (bh[i])
        {
            wait_on_buffer(bh[i]);           // 等待读取完成
            if (bh[i]->b_uptodate)           // 如果读取成功
                COPYBLK((unsigned long)bh[i]->b_data, address); // 复制数据
            brelse(bh[i]);                   // 释放缓冲区
        }
}

/*
 * Ok, breada 可以像bread一样使用，但另外标记其他块也要读取。
 * 用负数结束参数列表。
 */
struct buffer_head *breada(int dev, int first, ...)
{
    va_list args;
    struct buffer_head *bh, *tmp;

    va_start(args, first);
    if (!(bh = getblk(dev, first))) // 获取第一个缓冲区
        panic("bread: getblk returned NULL\n");
    if (!bh->b_uptodate)            // 如果数据不是最新的
        ll_rw_block(READ, bh);      // 发起读取请求
    
    // 预读其他块
    while ((first = va_arg(args, int)) >= 0)
    {
        tmp = getblk(dev, first);   // 获取缓冲区但不增加引用计数
        if (tmp)
        {
            if (!tmp->b_uptodate)   // 如果数据不是最新的
                ll_rw_block(READA, bh); // 发起预读请求（异步）
            tmp->b_count--;         // 减少引用计数（我们不持有它）
        }
    }
    va_end(args);
    
    wait_on_buffer(bh);             // 等待第一个缓冲区读取完成
    if (bh->b_uptodate)             // 如果读取成功
        return bh;
    brelse(bh);                     // 否则释放缓冲区
    return (NULL);                  // 返回NULL表示失败
}

// 缓冲区初始化函数
void buffer_init(long buffer_end)
{
    struct buffer_head *h = start_buffer;
    void *b;
    int i;

    // 确定缓冲区内存区域的结束位置
    if (buffer_end == 1 << 20)      // 如果缓冲区结束在1MB处
        b = (void *)(640 * 1024);   // 则从640KB开始（避开显存区域）
    else
        b = (void *)buffer_end;     // 否则从buffer_end开始
    
    // 初始化所有缓冲区头和数据区域
    while ((b -= BLOCK_SIZE) >= ((void *)(h + 1))) // 为每个缓冲区分配内存
    {
        h->b_dev = 0;               // 设备号：0表示空闲
        h->b_dirt = 0;              // 脏标志：0表示未修改
        h->b_count = 0;             // 引用计数：0表示未使用
        h->b_lock = 0;              // 锁定标志：0表示未锁定
        h->b_uptodate = 0;          // 最新标志：0表示数据可能过时
        h->b_wait = NULL;           // 等待队列：初始为空
        h->b_next = NULL;           // 哈希链表下一个指针
        h->b_prev = NULL;           // 哈希链表前一个指针
        h->b_data = (char *)b;      // 数据区域指针
        h->b_prev_free = h - 1;     // 空闲链表前一个指针
        h->b_next_free = h + 1;     // 空闲链表下一个指针
        h++;                        // 下一个缓冲区头
        NR_BUFFERS++;               // 增加缓冲区计数
        if (b == (void *)0x100000)  // 如果到达1MB边界
            b = (void *)0xA0000;    // 跳到0xA0000（640KB），避开显存
    }
    h--;                            // 调整指针
    
    // 设置空闲链表为循环链表
    free_list = start_buffer;
    free_list->b_prev_free = h;
    h->b_next_free = free_list;
    
    // 初始化哈希表
    for (i = 0; i < NR_HASH; i++)
        hash_table[i] = NULL;
}