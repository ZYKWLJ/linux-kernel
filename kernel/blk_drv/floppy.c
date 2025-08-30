/*
 *  linux/kernel/floppy.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 02.12.91 - 改为静态变量来表示需要重置和重新校准的情况。这样做让一些事情（如输出字节、重置检查等）更简单，并且在出错时减少中断跳转，代码有望更易理解。
 */

/*
 * 这个文件无疑很混乱。我已尽力让它能工作，但我不喜欢对软盘进行编程，而且我也只有一个软盘驱动器。唉。我应该检查更多错误，并进行更优雅的错误恢复。似乎多个驱动器存在问题，我已尝试修正。不保证没问题。
 */

/*
 * 和 hd.c 一样，这个文件中的所有例程都可能（并且会）被中断调用，所以需要极其谨慎。硬件中断处理程序不能睡眠，否则会发生内核 panic。因此我不能直接调用 “floppy - on”，而必须设置一个特殊的定时器中断等。
 *
 * 另外，我不确定这在多个软盘上是否能工作。可能有很多 bug。
 */

#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#define MAJOR_NR 2
#include "blk.h"

// 标记是否需要重新校准
static int recalibrate = 0;
// 标记是否需要重置
static int reset = 0;
// 标记是否需要寻道
static int seek = 0;

// 外部声明当前数字输出寄存器的值
extern unsigned char current_DOR;

// 立即输出字节到端口并做一些延迟操作的宏
#define immoutb_p(val, port) \
    __asm__("outb %0,%1\n\tjmp 1f\n1:\tjmp 1f\n1:" ::"a"((char)(val)), "i"(port))

// 获取驱动器类型
#define TYPE(x) ((x) >> 2)
// 获取驱动器号
#define DRIVE(x) ((x) & 0x03)
// 最大错误次数定义，注意：MAX_ERRORS = 8 并不意味着每次坏的读取最多重试 8 次——某些类型的错误会使错误计数增加 2，所以实际上可能只重试 5 - 6 次就放弃
#define MAX_ERRORS 8

/*
 * 全局变量，供 'result()' 使用
 */
// 最大回复数量
#define MAX_REPLIES 7
// 回复缓冲区
static unsigned char reply_buffer[MAX_REPLIES];
// 状态寄存器 0
#define ST0 (reply_buffer[0])
// 状态寄存器 1
#define ST1 (reply_buffer[1])
// 状态寄存器 2
#define ST2 (reply_buffer[2])
// 状态寄存器 3
#define ST3 (reply_buffer[3])

/*
 * 这个结构体定义了不同的软盘类型。与 minix 不同，linux 没有 “搜索正确类型” 的类型，因为那部分代码复杂且怪异。我已经在这个驱动程序上遇到了足够多的问题。
 *
 * 'stretch' 表示某些类型的磁道是否需要加倍（例如 1.2MB 驱动器中的 360kB 软盘等）。其他字段应该是自解释的。
 */
static struct floppy_struct
{
    // 容量、扇区数、磁头数、磁道数、磁道是否加倍标志
    unsigned int size, sect, head, track, stretch;
    // 间隙、速率、特定参数 1
    unsigned char gap, rate, spec1;
} floppy_type[] = {
    {0, 0, 0, 0, 0, 0x00, 0x00, 0x00},      /* 无测试 */
    {720, 9, 2, 40, 0, 0x2A, 0x02, 0xDF},   /* 360kB PC 软盘 */
    {2400, 15, 2, 80, 0, 0x1B, 0x00, 0xDF}, /* 1.2 MB AT 软盘 */
    {720, 9, 2, 40, 1, 0x2A, 0x02, 0xDF},   /* 720kB 驱动器中的 360kB 软盘 */
    {1440, 9, 2, 80, 0, 0x2A, 0x02, 0xDF},  /* 3.5" 720kB 软盘 */
    {720, 9, 2, 40, 1, 0x23, 0x01, 0xDF},   /* 1.2MB 驱动器中的 360kB 软盘 */
    {1440, 9, 2, 80, 0, 0x23, 0x01, 0xDF},  /* 1.2MB 驱动器中的 720kB 软盘 */
    {2880, 18, 2, 80, 0, 0x1B, 0x00, 0xCF}, /* 1.44MB 软盘 */
};
/*
 * 速率：0 表示 500kb/s，2 表示 300kbps，1 表示 250kbps
 * Spec1 是 0xSH，其中 S 是步进速率（F = 1ms，E = 2ms，D = 3ms 等），
 * H 是磁头卸载时间（1 = 16ms，2 = 32ms 等）
 *
 * Spec2 是 (HLD << 1 | ND)，其中 HLD 是磁头加载时间（1 = 2ms，2 = 4 ms 等），
 * ND 被设置表示不使用 DMA。硬编码为 6（HLD = 6ms，使用 DMA）。
 */

// 外部声明软盘中断处理函数和临时软盘区域
extern void floppy_interrupt(void);
extern char tmp_floppy_area[1024];

/*
 * 这些是全局变量，因为这是向中断提供信息的最简单方式。它们是当前请求使用的数据。
 */
// 当前特定参数 1、速率
static int cur_spec1 = -1;
static int cur_rate = -1;
// 当前软盘类型、驱动器、扇区、磁头、磁道、寻道磁道、当前磁道、命令
static struct floppy_struct *floppy = floppy_type;
static unsigned char current_drive = 0;
static unsigned char sector = 0;
static unsigned char head = 0;
static unsigned char track = 0;
static unsigned char seek_track = 0;
static unsigned char current_track = 255;
static unsigned char command = 0;
// 标记是否已选择驱动器，等待软盘选择的任务结构体
unsigned char selected = 0;
struct task_struct *wait_on_floppy_select = NULL;

// 取消选择软盘驱动器
void floppy_deselect(unsigned int nr)
{
    if (nr != (current_DOR & 3))
        printk("floppy_deselect: drive not selected\n\r");
    selected = 0;
    wake_up(&wait_on_floppy_select);
}

/*
 * floppy - change 从不会被中断调用，所以我们在这里可以放松一点，睡眠等。注意 floppy - on 尝试将 current_DOR 设置为指向所需的驱动器，但如果同时使用多个软盘，它可能无法在睡眠后保持：因此使用循环。
 */
int floppy_change(unsigned int nr)
{
repeat:
    floppy_on(nr);
    while ((current_DOR & 3) != nr && selected)
        interruptible_sleep_on(&wait_on_floppy_select);
    if ((current_DOR & 3) != nr)
        goto repeat;
    if (inb(FD_DIR) & 0x80)
    {
        floppy_off(nr);
        return 1;
    }
    floppy_off(nr);
    return 0;
}

// 复制缓冲区的宏
#define copy_buffer(from, to) \
    __asm__("cld ; rep ; movsl" ::"c"(BLOCK_SIZE / 4), "S"((long)(from)), "D"((long)(to)))

// 设置 DMA（直接内存访问）
static void setup_DMA(void)
{
    long addr = (long)CURRENT->buffer;

    cli();
    // 如果地址大于等于 0x100000，使用临时软盘区域
    if (addr >= 0x100000)
    {
        addr = (long)tmp_floppy_area;
        if (command == FD_WRITE)
            copy_buffer(CURRENT->buffer, tmp_floppy_area);
    }
    // 屏蔽 DMA 2
    immoutb_p(4 | 2, 10);
    // 输出命令字节。不知道为什么，每个人（minix、sanches & canton）都输出两次，先到 12 再到 11
    __asm__("outb %%al,$12\n\tjmp 1f\n1:\tjmp 1f\n1:\t"
            "outb %%al,$11\n\tjmp 1f\n1:\tjmp 1f\n1:" ::
                "a"((char)((command == FD_READ) ? DMA_READ : DMA_WRITE)));
    // 地址低 8 位
    immoutb_p(addr, 4);
    addr >>= 8;
    // 地址 8 - 15 位
    immoutb_p(addr, 4);
    addr >>= 8;
    // 地址 16 - 19 位
    immoutb_p(addr, 0x81);
    // 计数 - 1 的低 8 位（1024 - 1 = 0x3ff）
    immoutb_p(0xff, 5);
    // 计数 - 1 的高 8 位
    immoutb_p(3, 5);
    // 激活 DMA 2
    immoutb_p(0 | 2, 10);
    sti();
}

// 向软盘控制器输出字节
static void output_byte(char byte)
{
    int counter;
    unsigned char status;

    if (reset)
        return;
    for (counter = 0; counter < 10000; counter++)
    {
        status = inb_p(FD_STATUS) & (STATUS_READY | STATUS_DIR);
        if (status == STATUS_READY)
        {
            outb(byte, FD_DATA);
            return;
        }
    }
    reset = 1;
    printk("Unable to send byte to FDC\n\r");
}

// 获取软盘控制器的结果
static int result(void)
{
    int i = 0, counter, status;

    if (reset)
        return -1;
    for (counter = 0; counter < 10000; counter++)
    {
        status = inb_p(FD_STATUS) & (STATUS_DIR | STATUS_READY | STATUS_BUSY);
        if (status == STATUS_READY)
            return i;
        if (status == (STATUS_DIR | STATUS_READY | STATUS_BUSY))
        {
            if (i >= MAX_REPLIES)
                break;
            reply_buffer[i++] = inb_p(FD_DATA);
        }
    }
    reset = 1;
    printk("Getstatus times out\n\r");
    return -1;
}

// 处理软盘操作错误
static void bad_flp_intr(void)
{
    CURRENT->errors++;
    if (CURRENT->errors > MAX_ERRORS)
    {
        floppy_deselect(current_drive);
        end_request(0);
    }
    if (CURRENT->errors > MAX_ERRORS / 2)
        reset = 1;
    else
        recalibrate = 1;
}

/*
 * 好的，这个中断在 DMA 读/写成功后被调用，所以我们检查结果，并复制任何缓冲区。
 */
static void rw_interrupt(void)
{
    if (result() != 7 || (ST0 & 0xf8) || (ST1 & 0xbf) || (ST2 & 0x73))
    {
        if (ST1 & 0x02)
        {
            printk("Drive %d is write protected\n\r", current_drive);
            floppy_deselect(current_drive);
            end_request(0);
        }
        else
            bad_flp_intr();
        do_fd_request();
        return;
    }
    if (command == FD_READ && (unsigned long)(CURRENT->buffer) >= 0x100000)
        copy_buffer(tmp_floppy_area, CURRENT->buffer);
    floppy_deselect(current_drive);
    end_request(1);
    do_fd_request();
}

// 设置读写软盘
static inline void setup_rw_floppy(void)
{
    setup_DMA();
    do_floppy = rw_interrupt;
    output_byte(command);
    output_byte(head << 2 | current_drive);
    output_byte(track);
    output_byte(head);
    output_byte(sector);
    output_byte(2); /* 扇区大小 = 512 */
    output_byte(floppy->sect);
    output_byte(floppy->gap);
    output_byte(0xFF); /* 扇区大小（当 n != 0 时为 0xff？） */
    if (reset)
        do_fd_request();
}

/*
 * 这是每次寻道（或重新校准）中断后从软盘控制器调用的例程。注意“意外中断”例程也会进行重新校准，但不会来到这里。
 */
static void seek_interrupt(void)
{
    // 检测驱动器状态
    output_byte(FD_SENSEI);
    if (result() != 2 || (ST0 & 0xF8) != 0x20 || ST1 != seek_track)
    {
        bad_flp_intr();
        do_fd_request();
        return;
    }
    current_track = ST1;
    setup_rw_floppy();
}

/*
 * 当传输的所有准备工作都应该正确设置时（即软盘电机已开启且正确的软盘已被选择），调用这个例程。
 */
static void transfer(void)
{
    if (cur_spec1 != floppy->spec1)
    {
        cur_spec1 = floppy->spec1;
        output_byte(FD_SPECIFY);
        output_byte(cur_spec1); /* hut 等 */
        output_byte(6);         /* 磁头加载时间 = 6ms，使用 DMA */
    }
    if (cur_rate != floppy->rate)
        outb_p(cur_rate = floppy->rate, FD_DCR);
    if (reset)
    {
        do_fd_request();
        return;
    }
    if (!seek)
    {
        setup_rw_floppy();
        return;
    }
    do_floppy = seek_interrupt;
    if (seek_track)
    {
        output_byte(FD_SEEK);
        output_byte(head << 2 | current_drive);
        output_byte(seek_track);
    }
    else
    {
        output_byte(FD_RECALIBRATE);
        output_byte(head << 2 | current_drive);
    }
    if (reset)
        do_fd_request();
}

/*
 * 特殊情况——在意外中断（或重置）后使用
 */
static void recal_interrupt(void)
{
    output_byte(FD_SENSEI);
    if (result() != 2 || (ST0 & 0xE0) == 0x60)
        reset = 1;
    else
        recalibrate = 0;
    do_fd_request();
}

// 处理意外的软盘中断
void unexpected_floppy_interrupt(void)
{
    output_byte(FD_SENSEI);
    if (result() != 2 || (ST0 & 0xE0) == 0x60)
        reset = 1;
    else
        recalibrate = 1;
}

// 重新校准软盘
static void recalibrate_floppy(void)
{
    recalibrate = 0;
    current_track = 0;
    do_floppy = recal_interrupt;
    output_byte(FD_RECALIBRATE);
    output_byte(head << 2 | current_drive);
    if (reset)
        do_fd_request();
}

// 重置中断处理
static void reset_interrupt(void)
{
    output_byte(FD_SENSEI);
    (void)result();
    output_byte(FD_SPECIFY);
    output_byte(cur_spec1); /* hut 等 */
    output_byte(6);         /* 磁头加载时间 = 6ms，使用 DMA */
    do_fd_request();
}

/*
 * 重置是通过将 DOR 的位 2 拉低一段时间来完成的。
 */
static void reset_floppy(void)
{