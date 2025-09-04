/*
 *  linux/fs/super.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 文件头注释：说明文件名、作者和版权信息
 * super.c 包含处理超级块表的代码。
 */
#include <linux/config.h> // 内核配置头文件，包含各种配置宏
#include <linux/sched.h>  // 调度程序头文件，定义任务结构task_struct、睡眠函数等
#include <linux/kernel.h> // 内核头文件，包含内核函数声明如printk
#include <asm/system.h>   // 系统头文件，包含cli、sti等汇编指令定义

#include <errno.h>    // 错误号定义头文件
#include <sys/stat.h> // 文件状态头文件，定义S_ISBLK、S_ISDIR等宏

// 声明外部函数
int sync_dev(int dev);        // 同步设备缓冲区函数（在buffer.c中定义）
void wait_for_keypress(void); // 等待按键函数（在console.c中定义）

/*
 * set_bit 使用 setb 指令，因为 gas 汇编器不识别 setc
 * 内联汇编：设置指定位
 * 参数：bitnr - 位号，addr - 地址
 * 返回值：设置前该位的值（0或1）
 */
#define set_bit(bitnr, addr) ({ \
register int __res ; \  // 定义寄存器变量__res存储结果
__asm__("bt %2,%3;setb %%al" : "=a"(__res) : "a"(0), "r"(bitnr), "m"(*(addr)));
__res;
})  // 汇编指令：bt测试位，setb根据进位标志设置al寄存器

// 定义超级块数组，NR_SUPER是系统允许的最大超级块数量（通常为8）
struct super_block super_block[NR_SUPER];

/* 根设备号，在init/main.c中初始化 */
int ROOT_DEV = 0;

/*
 * 锁定超级块
 * 参数：sb - 要锁定的超级块指针
 */
static void lock_super(struct super_block *sb)
{
    cli();                       // 关中断，进入临界区
    while (sb->s_lock)           // 如果超级块已被锁定
        sleep_on(&(sb->s_wait)); // 当前进程睡眠在超级块的等待队列上
    sb->s_lock = 1;              // 设置锁定标志
    sti();                       // 开中断
}

/*
 * 释放超级块锁
 * 参数：sb - 要释放的超级块指针
 */
static void free_super(struct super_block *sb)
{
    cli();                  // 关中断
    sb->s_lock = 0;         // 清除锁定标志
    wake_up(&(sb->s_wait)); // 唤醒等待该超级块的所有进程
    sti();                  // 开中断
}

/*
 * 等待超级块解锁
 * 参数：sb - 要等待的超级块指针
 */
static void wait_on_super(struct super_block *sb)
{
    cli();                       // 关中断
    while (sb->s_lock)           // 如果超级块被锁定
        sleep_on(&(sb->s_wait)); // 当前进程睡眠等待
    sti();                       // 开中断
}

/*
 * 获取指定设备的超级块
 * 参数：dev - 设备号
 * 返回：超级块指针或NULL
 */
struct super_block *get_super(int dev)
{
    struct super_block *s;

    if (!dev) // 如果设备号为0，返回NULL
        return NULL;
    s = 0 + super_block;               // s指向超级块数组起始地址（等价于s = super_block）
    while (s < NR_SUPER + super_block) // 遍历超级块数组
        if (s->s_dev == dev)           // 找到指定设备的超级块
        {
            wait_on_super(s);    // 等待超级块解锁
            if (s->s_dev == dev) // 再次检查设备号（防止等待期间超级块被释放）
                return s;
            s = 0 + super_block; // 如果设备号改变，重新开始搜索
        }
        else
            s++; // 检查下一个超级块
    return NULL; // 未找到
}

/*
 * 释放指定设备的超级块资源
 * 参数：dev - 设备号
 */
void put_super(int dev)
{
    struct super_block *sb;
    int i;

    if (dev == ROOT_DEV) // 根设备不能释放
    {
        printk("root diskette changed: prepare for armageddon\n\r");
        return;
    }
    if (!(sb = get_super(dev))) // 获取超级块，如果不存在则返回
        return;
    if (sb->s_imount) // 如果超级块有挂载的文件系统
    {
        printk("Mounted disk changed - tssk, tssk\n\r");
        return;
    }
    lock_super(sb); // 锁定超级块
    sb->s_dev = 0;  // 清除设备号，标记该超级块空闲
    // 释放i节点位图缓冲区
    for (i = 0; i < I_MAP_SLOTS; i++)
        brelse(sb->s_imap[i]);
    // 释放块位图缓冲区
    for (i = 0; i < Z_MAP_SLOTS; i++)
        brelse(sb->s_zmap[i]);
    free_super(sb); // 释放超级块锁
    return;
}

/*
 * 从设备读取超级块到内存
 * 参数：dev - 设备号
 * 返回：超级块指针或NULL
 */
static struct super_block *read_super(int dev)
{
    struct super_block *s;
    struct buffer_head *bh;
    int i, block;

    if (!dev) // 设备号为0则返回NULL
        return NULL;
    check_disk_change(dev);   // 检查磁盘是否更换（如软盘）
    if ((s = get_super(dev))) // 如果超级块已在内存中，直接返回
        return s;
    // 寻找一个空闲的超级块槽位
    for (s = 0 + super_block;; s++)
    {
        if (s >= NR_SUPER + super_block) // 遍历完所有槽位
            return NULL;
        if (!s->s_dev) // 找到空闲槽位（s_dev为0）
            break;
    }
    s->s_dev = dev;     // 设置设备号
    s->s_isup = NULL;   // 初始化挂载的根i节点指针
    s->s_imount = NULL; // 初始化挂载点i节点指针
    s->s_time = 0;      // 初始化超级块修改时间
    s->s_rd_only = 0;   // 初始化只读标志
    s->s_dirt = 0;      // 初始化脏标志
    lock_super(s);      // 锁定超级块
    // 读取磁盘上第1块（超级块所在块，0是引导块）
    if (!(bh = bread(dev, 1)))
    {
        s->s_dev = 0; // 读取失败，释放超级块
        free_super(s);
        return NULL;
    }
    // 将磁盘超级块数据复制到内存超级块（强制类型转换）
    *((struct d_super_block *)s) =
        *((struct d_super_block *)bh->b_data);
    brelse(bh); // 释放缓冲区
    // 检查魔数，验证是否是有效的文件系统
    if (s->s_magic != SUPER_MAGIC)
    {
        s->s_dev = 0;
        free_super(s);
        return NULL;
    }
    // 初始化位图指针数组为NULL
    for (i = 0; i < I_MAP_SLOTS; i++)
        s->s_imap[i] = NULL;
    for (i = 0; i < Z_MAP_SLOTS; i++)
        s->s_zmap[i] = NULL;
    // 读取i节点位图块（从块2开始）
    block = 2;
    for (i = 0; i < s->s_imap_blocks; i++)
        if ((s->s_imap[i] = bread(dev, block)))
            block++;
        else
            break;
    // 读取块位图块
    for (i = 0; i < s->s_zmap_blocks; i++)
        if ((s->s_zmap[i] = bread(dev, block)))
            block++;
        else
            break;
    // 检查是否成功读取所有位图块
    if (block != 2 + s->s_imap_blocks + s->s_zmap_blocks)
    {
        // 读取失败，释放已分配的位图缓冲区
        for (i = 0; i < I_MAP_SLOTS; i++)
            brelse(s->s_imap[i]);
        for (i = 0; i < Z_MAP_SLOTS; i++)
            brelse(s->s_zmap[i]);
        s->s_dev = 0;
        free_super(s);
        return NULL;
    }
    // 设置位图的第0位（保留，不使用0号i节点和0号块）
    s->s_imap[0]->b_data[0] |= 1;
    s->s_zmap[0]->b_data[0] |= 1;
    free_super(s); // 释放超级块锁
    return s;
}

/*
 * 系统调用 - 卸载文件系统
 * 参数：dev_name - 设备文件名
 * 返回：0成功，负的错误码
 */
int sys_umount(char *dev_name)
{
    struct m_inode *inode;
    struct super_block *sb;
    int dev;

    // 获取设备文件对应的i节点
    if (!(inode = namei(dev_name)))
        return -ENOENT;          // 设备文件不存在
    dev = inode->i_zone[0];      // 从i节点中获取设备号（块设备的i_zone[0]存储设备号）
    if (!S_ISBLK(inode->i_mode)) // 检查是否是块设备
    {
        iput(inode);
        return -ENOTBLK; // 不是块设备
    }
    iput(inode);         // 释放设备文件i节点
    if (dev == ROOT_DEV) // 不能卸载根设备
        return -EBUSY;
    // 获取超级块，检查是否有挂载的文件系统
    if (!(sb = get_super(dev)) || !(sb->s_imount))
        return -ENOENT;
    if (!sb->s_imount->i_mount) // 一致性检查（应该为1）
        printk("Mounted inode has i_mount=0\n");
    // 检查是否有进程正在使用该设备上的文件
    for (inode = inode_table + 0; inode < inode_table + NR_INODE; inode++)
        if (inode->i_dev == dev && inode->i_count)
            return -EBUSY; // 设备忙，有i节点正在使用
    // 清除挂载标志
    sb->s_imount->i_mount = 0;
    iput(sb->s_imount); // 释放挂载点i节点
    sb->s_imount = NULL;
    iput(sb->s_isup); // 释放文件系统根i节点
    sb->s_isup = NULL;
    put_super(dev); // 释放超级块资源
    sync_dev(dev);  // 同步设备缓冲区
    return 0;
}

/*
 * 系统调用 - 挂载文件系统
 * 参数：dev_name - 设备文件名，dir_name - 挂载目录名，rw_flag - 读写标志
 * 返回：0成功，负的错误码
 */
int sys_mount(char *dev_name, char *dir_name, int rw_flag)
{
    struct m_inode *dev_i, *dir_i;
    struct super_block *sb;
    int dev;

    // 获取设备文件i节点
    if (!(dev_i = namei(dev_name)))
        return -ENOENT;
    dev = dev_i->i_zone[0];      // 获取设备号
    if (!S_ISBLK(dev_i->i_mode)) // 检查是否是块设备
    {
        iput(dev_i);
        return -EPERM; // 权限错误
    }
    iput(dev_i); // 释放设备文件i节点
    // 获取挂载目录i节点
    if (!(dir_i = namei(dir_name)))
        return -ENOENT;
    // 检查挂载目录：引用计数必须为1（只有当前引用），且不能是根i节点
    if (dir_i->i_count != 1 || dir_i->i_num == ROOT_INO)
    {
        iput(dir_i);
        return -EBUSY; // 目录忙
    }
    if (!S_ISDIR(dir_i->i_mode)) // 检查是否是目录
    {
        iput(dir_i);
        return -EPERM; // 权限错误
    }
    // 读取设备超级块
    if (!(sb = read_super(dev)))
    {
        iput(dir_i);
        return -EBUSY; // 设备忙或无效
    }
    if (sb->s_imount) // 检查文件系统是否已挂载
    {
        iput(dir_i);
        return -EBUSY;
    }
    if (dir_i->i_mount) // 检查目录是否已是挂载点
    {
        iput(dir_i);
        return -EPERM;
    }
    sb->s_imount = dir_i; // 设置超级块的挂载点i节点
    dir_i->i_mount = 1;   // 设置目录的挂载标志
    dir_i->i_dirt = 1;    // 标记目录i节点为脏（需要写回磁盘）
    // 注意：这里没有调用iput(dir_i)，将在umount时释放
    return 0;
}

/*
 * 挂载根文件系统（系统初始化时调用）
 */
void mount_root(void)
{
    int i, free;
    struct super_block *p;
    struct m_inode *mi;

    // 检查i节点结构大小是否正确（编译时检查）
    if (32 != sizeof(struct d_inode))
        panic("bad i-node size");
    // 初始化文件表（文件描述符表）
    for (i = 0; i < NR_FILE; i++)
        file_table[i].f_count = 0;
    // 如果是软驱设备，提示插入根文件系统软盘
    if (MAJOR(ROOT_DEV) == 2) // 主设备号2是软驱
    {
        printk("Insert root floppy and press ENTER");
        wait_for_keypress(); // 等待按键
    }
    // 初始化所有超级块
    for (p = &super_block[0]; p < &super_block[NR_SUPER]; p++)
    {
        p->s_dev = 0;     // 标记为空闲
        p->s_lock = 0;    // 未锁定
        p->s_wait = NULL; // 等待队列为空
    }
    // 读取根设备超级块
    if (!(p = read_super(ROOT_DEV)))
        panic("Unable to mount root"); // 失败则宕机
    // 获取根i节点（ROOT_INO通常为1）
    if (!(mi = iget(ROOT_DEV, ROOT_INO)))
        panic("Unable to read root i-node");
    mi->i_count += 3;             // 增加引用计数：当前进程、根目录、工作目录
    p->s_isup = p->s_imount = mi; // 设置超级块的根i节点和挂载点
    current->pwd = mi;            // 设置当前进程的工作目录
    current->root = mi;           // 设置当前进程的根目录
    // 计算空闲块数量
    free = 0;
    i = p->s_nzones; // 文件系统总块数
    while (--i >= 0)
        if (!set_bit(i & 8191, p->s_zmap[i >> 13]->b_data))
            free++; // 如果位为0，表示空闲块
    printk("%d/%d free blocks\n\r", free, p->s_nzones);
    // 计算空闲i节点数量
    free = 0;
    i = p->s_ninodes + 1; // 文件系统总i节点数+1
    while (--i >= 0)
        if (!set_bit(i & 8191, p->s_imap[i >> 13]->b_data))
            free++; // 如果位为0，表示空闲i节点
    printk("%d/%d free inodes\n\r", free, p->s_ninodes);
}