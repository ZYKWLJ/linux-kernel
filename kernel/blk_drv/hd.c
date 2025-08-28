/*
 *  linux/kernel/hd.c
 *
 *  (C) 1991  Linus Torvalds  // 版权声明，Linus Torvalds为作者
 */

/*
 * 这是硬盘底层中断支持代码。它通过中断在函数间跳转来遍历请求列表。
 * 由于所有函数都在中断上下文中调用，因此不能进行睡眠操作。使用时需特别注意。
 *
 * 由Drew Eckhardt修改，添加了从CMOS中检测硬盘数量的功能
 */

// 包含必要的头文件
#include <linux/config.h> // 内核配置相关宏定义
#include <linux/sched.h>  // 进程调度相关结构体和函数
#include <linux/fs.h>     // 文件系统相关定义
#include <linux/kernel.h> // 内核核心函数和宏
#include <linux/hdreg.h>  // 硬盘寄存器相关定义
#include <asm/system.h>   // 系统相关汇编操作
#include <asm/io.h>       // IO端口操作函数
#include <asm/segment.h>  // 段寄存器操作相关

#define MAJOR_NR 3 // 硬盘设备的主设备号(3为Linux中硬盘的标准主设备号)
#include "blk.h"   // 块设备通用处理头文件

// 从CMOS中读取指定地址的数据
// CMOS是主板上的一块存储芯片，保存硬件配置信息
#define CMOS_READ(addr) ({     \
    outb_p(0x80 | addr, 0x70); \  // 向0x70端口发送带最高位的地址(0x80用于禁止NMI)
inb_p(0x71);
\ // 从0x71端口读取数据
})

/* 每个扇区的最大读写错误次数 */
#define MAX_ERRORS 7
/* 最大硬盘数量 */
#define MAX_HD 2

// 函数声明：重新校准中断处理函数
static void recal_intr(void);

// 重新校准标志：1表示需要重新校准硬盘
static int recalibrate = 0;
// 重置标志：1表示需要重置硬盘控制器
static int reset = 0;

/*
 * 这个结构体定义了硬盘及其类型信息
 */
struct hd_i_struct
{
    int head;  // 磁头数
    int sect;  // 每磁道扇区数
    int cyl;   // 柱面数
    int wpcom; // 写预补偿
    int lzone; // 着陆区
    int ctl;   // 控制字节
};

// 根据是否定义HD_TYPE来初始化硬盘信息结构体
#ifdef HD_TYPE
// 如果定义了HD_TYPE，使用其初始化硬盘信息
struct hd_i_struct hd_info[] = {HD_TYPE};
// 计算硬盘数量
#define NR_HD ((sizeof(hd_info)) / (sizeof(struct hd_i_struct)))
#else
// 否则初始化为空，并动态检测硬盘数量
struct hd_i_struct hd_info[] = {{0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}};
static int NR_HD = 0;
#endif

// 硬盘分区信息结构体
static struct hd_struct
{
    long start_sect; // 分区起始扇区
    long nr_sects;   // 分区总扇区数
} hd[5 * MAX_HD] = {
    // 每个硬盘最多4个主分区+1个扩展分区，共5个
    {0, 0}, // 初始化为0
};

// 从指定端口读取数据到缓冲区
// 参数：port-端口号，buf-缓冲区，nr-读取的字数(16位)
#define port_read(port, buf, nr) \
    __asm__("cld;rep;insw" ::"d"(port), "D"(buf), "c"(nr))
// cld: 清除方向标志，使字符串操作递增
// rep;insw: 重复从DX端口读取字到ES:DI指向的内存，共CX次

// 从缓冲区写数据到指定端口
#define port_write(port, buf, nr) \
    __asm__("cld;rep;outsw" ::"d"(port), "S"(buf), "c"(nr))
// outsw: 从DS:SI指向的内存写数据到DX端口

// 外部函数声明
extern void hd_interrupt(void); // 硬盘中断处理函数
extern void rd_load(void);      // 虚拟盘加载函数

/* 此函数只能被调用一次，由static int callable保证 */
int sys_setup(void *BIOS)
{
    static int callable = 1; // 确保函数只被调用一次的标志
    int i, drive;
    unsigned char cmos_disks; // 从CMOS读取的硬盘信息
    struct partition *p;      // 分区结构体指针
    struct buffer_head *bh;   // 缓冲区头指针

    // 如果已经被调用过，返回-1
    if (!callable)
        return -1;
    callable = 0; // 标记为已调用

#ifndef HD_TYPE
    // 从BIOS获取硬盘参数(硬盘类型、磁头数等)
    for (drive = 0; drive < 2; drive++)
    {
        hd_info[drive].cyl = *(unsigned short *)BIOS;          // 柱面数
        hd_info[drive].head = *(unsigned char *)(2 + BIOS);    // 磁头数
        hd_info[drive].wpcom = *(unsigned short *)(5 + BIOS);  // 写预补偿
        hd_info[drive].ctl = *(unsigned char *)(8 + BIOS);     // 控制字节
        hd_info[drive].lzone = *(unsigned short *)(12 + BIOS); // 着陆区
        hd_info[drive].sect = *(unsigned char *)(14 + BIOS);   // 每磁道扇区数
        BIOS += 16;                                            // 移动到下一个硬盘的参数
    }
    // 根据第二个硬盘的柱面数判断硬盘数量
    if (hd_info[1].cyl)
        NR_HD = 2;
    else
        NR_HD = 1;
#endif

    // 初始化硬盘分区信息(整个硬盘作为一个分区)
    for (i = 0; i < NR_HD; i++)
    {
        hd[i * 5].start_sect = 0; // 起始扇区为0
        // 计算总扇区数=磁头数×每磁道扇区数×柱面数
        hd[i * 5].nr_sects = hd_info[i].head *
                             hd_info[i].sect * hd_info[i].cyl;
    }

    /*
        我们查询CMOS获取硬盘信息：可能存在与ST-506兼容的
        SCSI/ESDI等控制器，它们会出现在BIOS表中，但可能
        不兼容寄存器，因此不会出现在CMOS中。

        此外，我们假设ST-506驱动器(如果有的话)是系统中的
        主驱动器，对应驱动器1或2。

        第一个驱动器信息存储在CMOS 0x12字节的高四位，第二个在低四位。
        这可以是4位驱动器类型或0xf，表示使用CMOS中0x19字节(驱动器1)
        或0x1a字节(驱动器2)的8位类型。

        显然，非零值意味着我们有对应驱动器的AT控制器硬盘。
    */

    // 从CMOS读取硬盘配置信息(0x12号地址)
    if ((cmos_disks = CMOS_READ(0x12)) & 0xf0)
        if (cmos_disks & 0x0f)
            NR_HD = 2; // 两个硬盘
        else
            NR_HD = 1; // 一个硬盘
    else
        NR_HD = 0; // 无硬盘

    // 初始化未使用的硬盘分区信息
    for (i = NR_HD; i < 2; i++)
    {
        hd[i * 5].start_sect = 0;
        hd[i * 5].nr_sects = 0;
    }

    // 读取每个硬盘的分区表
    for (drive = 0; drive < NR_HD; drive++)
    {
        // 读取硬盘的分区表(位于0x300+drive*5设备的第0块)
        if (!(bh = bread(0x300 + drive * 5, 0)))
        {
            printk("无法读取驱动器 %d 的分区表\n\r", drive);
            panic(""); // 严重错误，系统崩溃
        }
        // 检查分区表有效性(最后两个字节应为0x55AA)
        if (bh->b_data[510] != 0x55 || (unsigned char)
                                               bh->b_data[511] != 0xAA)
        {
            printk("驱动器 %d 上的分区表损坏\n\r", drive);
            panic("");
        }
        // 指向分区表项(每个分区表项从0x1BE开始，共4个)
        p = 0x1BE + (void *)bh->b_data;
        // 读取4个分区表项
        for (i = 1; i < 5; i++, p++)
        {
            hd[i + 5 * drive].start_sect = p->start_sect; // 分区起始扇区
            hd[i + 5 * drive].nr_sects = p->nr_sects;     // 分区扇区数
        }
        brelse(bh); // 释放缓冲区
    }
    // 打印分区表状态
    if (NR_HD)
        printk("分区表%s正常。\n\r", (NR_HD > 1) ? "s" : "");
    rd_load();    // 加载虚拟盘
    mount_root(); // 挂载根文件系统
    return (0);
}

// 检查硬盘控制器是否就绪
static int controller_ready(void)
{
    int retries = 100000; // 重试次数

    // 等待控制器就绪(状态寄存器的BUSY位为0)
    while (--retries && (inb_p(HD_STATUS) & 0x80))
        ;
    return (retries); // 返回剩余重试次数，0表示超时
}

// 获取硬盘操作结果状态
static int win_result(void)
{
    int i = inb_p(HD_STATUS); // 读取状态寄存器

    // 检查状态：就绪且寻道完成表示操作成功
    if ((i & (BUSY_STAT | READY_STAT | WRERR_STAT | SEEK_STAT | ERR_STAT)) ==
        (READY_STAT | SEEK_STAT))
        return (0); /* 操作成功 */

    // 如果有错误，读取错误寄存器
    if (i & 1)
        i = inb(HD_ERROR);
    return (1); // 操作失败
}

// 向硬盘控制器发送命令和参数
static void hd_out(unsigned int drive, unsigned int nsect, unsigned int sect,
                   unsigned int head, unsigned int cyl, unsigned int cmd,
                   void (*intr_addr)(void))
{
    register int port asm("dx"); // 端口号寄存器

    // 检查参数合法性
    if (drive > 1 || head > 15)
        panic("尝试写入错误的扇区");
    // 检查控制器是否就绪
    if (!controller_ready())
        panic("硬盘控制器未就绪");

    do_hd = intr_addr;                  // 设置中断处理函数
    outb_p(hd_info[drive].ctl, HD_CMD); // 发送控制字节

    port = HD_DATA;                            // 数据端口基地址
    outb_p(hd_info[drive].wpcom >> 2, ++port); // 写预补偿
    outb_p(nsect, ++port);                     // 扇区数
    outb_p(sect, ++port);                      // 起始扇区
    outb_p(cyl, ++port);                       // 柱面号低8位
    outb_p(cyl >> 8, ++port);                  // 柱面号高8位
    // 驱动器号和磁头号(0xA0表示主盘，0xB0表示从盘)
    outb_p(0xA0 | (drive << 4) | head, ++port);
    outb(cmd, ++port); // 发送命令
}

// 检查驱动器是否繁忙
static int drive_busy(void)
{
    unsigned int i;

    // 等待驱动器就绪(最多10000次循环)
    for (i = 0; i < 10000; i++)
        if (READY_STAT == (inb_p(HD_STATUS) & (BUSY_STAT | READY_STAT)))
            break;

    i = inb(HD_STATUS); // 读取状态
    i &= BUSY_STAT | READY_STAT | SEEK_STAT;
    // 就绪且寻道完成表示不繁忙
    if (i == (READY_STAT | SEEK_STAT))
        return (0);
    printk("硬盘控制器超时\n\r");
    return (1);
}

// 重置硬盘控制器
static void reset_controller(void)
{
    int i;

    outb(4, HD_CMD); // 发送重置命令
    for (i = 0; i < 100; i++)
        nop(); // 延迟一小段时间
    // 恢复控制字节(低4位)
    outb(hd_info[0].ctl & 0x0f, HD_CMD);

    // 检查控制器是否仍繁忙
    if (drive_busy())
        printk("硬盘控制器仍然繁忙\n\r");
    // 检查重置结果
    if ((i = inb(HD_ERROR)) != 1)
        printk("硬盘控制器重置失败: %02x\n\r", i);
}

// 重置指定硬盘
static void reset_hd(int nr)
{
    reset_controller(); // 重置控制器
    // 发送指定命令，设置重新校准中断处理函数
    hd_out(nr, hd_info[nr].sect, hd_info[nr].sect, hd_info[nr].head - 1,
           hd_info[nr].cyl, WIN_SPECIFY, &recal_intr);
}

// 处理意外的硬盘中断
void unexpected_hd_interrupt(void)
{
    printk("意外的硬盘中断\n\r");
}

// 处理读写错误中断
static void bad_rw_intr(void)
{
    // 如果错误次数超过最大值，结束请求(失败)
    if (++CURRENT->errors >= MAX_ERRORS)
        end_request(0);
    // 如果错误次数超过一半，标记需要重置
    if (CURRENT->errors > MAX_ERRORS / 2)
        reset = 1;
}

// 读取操作中断处理函数
static void read_intr(void)
{
    // 检查操作结果
    if (win_result())
    {
        bad_rw_intr();   // 处理错误
        do_hd_request(); // 处理下一个请求
        return;
    }
    // 从硬盘读取数据到缓冲区(256个字=512字节)
    port_read(HD_DATA, CURRENT->buffer, 256);
    CURRENT->errors = 0;    // 重置错误计数
    CURRENT->buffer += 512; // 移动缓冲区指针
    CURRENT->sector++;      // 扇区号递增
    // 如果还有扇区要读，继续设置读取中断
    if (--CURRENT->nr_sectors)
    {
        do_hd = &read_intr;
        return;
    }
    end_request(1);  // 成功结束请求
    do_hd_request(); // 处理下一个请求
}

// 写入操作中断处理函数
static void write_intr(void)
{
    // 检查操作结果
    if (win_result())
    {
        bad_rw_intr();   // 处理错误
        do_hd_request(); // 处理下一个请求
        return;
    }
    // 如果还有扇区要写
    if (--CURRENT->nr_sectors)
    {
        CURRENT->sector++;                         // 扇区号递增
        CURRENT->buffer += 512;                    // 移动缓冲区指针
        do_hd = &write_intr;                       // 设置下一次中断处理函数
        port_write(HD_DATA, CURRENT->buffer, 256); // 继续写入数据
        return;
    }
    end_request(1);  // 成功结束请求
    do_hd_request(); // 处理下一个请求
}

// 重新校准中断处理函数
static void recal_intr(void)
{
    if (win_result())  // 检查校准结果
        bad_rw_intr(); // 处理错误
    do_hd_request();   // 处理下一个请求
}

// 硬盘请求处理函数
void do_hd_request(void)
{
    int i, r = 0;
    unsigned int block, dev;
    unsigned int sec, head, cyl; // 扇区、磁头、柱面
    unsigned int nsect;          // 扇区数

    INIT_REQUEST;              // 初始化请求，检查当前请求是否有效
    dev = MINOR(CURRENT->dev); // 获取次设备号
    block = CURRENT->sector;   // 获取起始块号

    // 检查设备号和块号是否合法
    if (dev >= 5 * NR_HD || block + 2 > hd[dev].nr_sects)
    {
        end_request(0); // 结束请求(失败)
        goto repeat;    // 处理下一个请求
    }

    // 计算绝对块号(加上分区起始扇区)
    block += hd[dev].start_sect;
    dev /= 5; // 计算硬盘号(每个硬盘有5个分区项)

    // 计算扇区号：block = (柱面数×磁头数 + 磁头)×每磁道扇区数 + 扇区 - 1
    // 第一步：计算扇区号
    __asm__("divl %4" : "=a"(block), "=d"(sec) : "0"(block), "1"(0),
                                                 "r"(hd_info[dev].sect));
    // 第二步：计算柱面号和磁头号
    __asm__("divl %4" : "=a"(cyl), "=d"(head) : "0"(block), "1"(0),
                                                "r"(hd_info[dev].head));
    sec++;                       // 扇区号从1开始(硬盘扇区编号从1开始)
    nsect = CURRENT->nr_sectors; // 要传输的扇区数

    // 如果需要重置硬盘
    if (reset)
    {
        reset = 0;
        recalibrate = 1;       // 标记需要重新校准
        reset_hd(CURRENT_DEV); // 重置当前设备
        return;
    }

    // 如果需要重新校准
    if (recalibrate)
    {
        recalibrate = 0;
        // 发送重新校准命令
        hd_out(dev, hd_info[CURRENT_DEV].sect, 0, 0, 0,
               WIN_RESTORE, &recal_intr);
        return;
    }

    // 处理写请求
    if (CURRENT->cmd == WRITE)
    {
        // 发送写命令，设置写中断处理函数
        hd_out(dev, nsect, sec, head, cyl, WIN_WRITE, &write_intr);
        // 等待数据请求就绪(DRQ位)
        for (i = 0; i < 3000 && !(r = inb_p(HD_STATUS) & DRQ_STAT); i++)
            /* 空循环等待 */;
        // 如果超时未就绪，处理错误
        if (!r)
        {
            bad_rw_intr();
            goto repeat;
        }
        // 向硬盘写入数据(256个字=512字节)
        port_write(HD_DATA, CURRENT->buffer, 256);
    }
    // 处理读请求
    else if (CURRENT->cmd == READ)
    {
        // 发送读命令，设置读中断处理函数
        hd_out(dev, nsect, sec, head, cyl, WIN_READ, &read_intr);
    }
    else
        panic("未知的硬盘命令"); // 处理未知命令

repeat:
    do_hd_request(); // 处理下一个请求
}

// 硬盘初始化函数
void hd_init(void)
{
    // 设置块设备请求处理函数
    blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
    // 设置硬盘中断门(0x2E为硬盘中断向量)
    set_intr_gate(0x2E, &hd_interrupt);
    // 允许主8259A的IRQ14中断(硬盘中断)
    outb_p(inb_p(0x21) & 0xfb, 0x21);
    // 允许从8259A的相应中断
    outb(inb_p(0xA1) & 0xbf, 0xA1);
}
