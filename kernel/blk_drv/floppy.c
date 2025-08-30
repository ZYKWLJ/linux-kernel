/*
 *  linux/kernel/floppy.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 02.12.91 - 改为使用静态变量来指示需要复位和重新校准
 * 这使得某些事情更容易（如output_byte中的复位检查等），
 * 并且在错误情况下中断跳转更少，所以代码希望更容易理解。
 */

/*
 * 这个文件确实很乱。我已经尽力让它工作，
 * 但我不喜欢编程软盘，而且我只有一个。
 * 唉。我应该检查更多错误，并进行更优雅的错误恢复。
 * 似乎有几个驱动器有问题。我已经尝试纠正它们。不做任何承诺。
 */

/*
 * 与hd.c一样，此文件中的所有例程都可以（并且将会）被中断调用，
 * 因此需要极其谨慎。硬件中断处理程序不能睡眠，否则会发生内核恐慌。
 * 因此我不能直接调用"floppy-on"，而是必须设置一个特殊的定时器中断等。
 *
 * 另外，我不确定这能否在多于一个软盘上工作。可能有很多错误。
 */

#include <linux/sched.h>    // 调度程序头文件，定义了任务结构task_struct、任务0数据等
#include <linux/fs.h>       // 文件系统头文件，定义文件表结构（file,buffer_head,m_inode等）
#include <linux/kernel.h>   // 内核头文件，含有一些内核常用函数的原形定义
#include <linux/fdreg.h>    // 软盘头文件，含有软盘控制器参数的一些定义
#include <asm/system.h>     // 系统头文件，定义了设置或修改描述符/中断门等的嵌入式汇编宏
#include <asm/io.h>         // io头文件，定义硬件端口输入/输出宏汇编语句
#include <asm/segment.h>    // 段操作头文件，定义了有关段寄存器操作的嵌入式汇编函数

#define MAJOR_NR 2          // 软驱的主设备号是2
#include "blk.h"            // 块设备头文件，定义请求数据结构、宏等

static int recalibrate = 0; // 重新校准标志。需要重新校准时为真
static int reset = 0;       // 复位标志。需要复位软盘控制器时为真
static int seek = 0;        // 寻道标志。需要寻道时为真

extern unsigned char current_DOR; // 当前数字输出寄存器（Digital Output Register）

// 带延迟的输出字节宏。val - 输出值；port - 端口
#define immoutb_p(val, port) \
    __asm__("outb %0,%1\n\tjmp 1f\n1:\tjmp 1f\n1:" ::"a"((char)(val)), "i"(port))

// 取软盘类型（类型码在次设备号的高2位中）
#define TYPE(x) ((x) >> 2)
// 取软盘驱动器号（在次设备号的低2位中）
#define DRIVE(x) ((x) & 0x03)

/*
 * 注意MAX_ERRORS=8并不意味着我们对每个坏读最多重试8次 -
 * 某些类型的错误会使错误计数增加2，所以我们实际上可能只重试5-6次就放弃了。
 */
#define MAX_ERRORS 8        // 最大错误次数

/*
 * 由'result()'使用的全局变量
 */
#define MAX_REPLIES 7       // 最大返回结果字节数
static unsigned char reply_buffer[MAX_REPLIES]; // 结果字节缓冲区
#define ST0 (reply_buffer[0])   // 状态0
#define ST1 (reply_buffer[1])   // 状态1
#define ST2 (reply_buffer[2])   // 状态2
#define ST3 (reply_buffer[3])   // 状态3

/*
 * 这个结构定义了不同的软盘类型。与minix不同，
 * linux没有"搜索正确类型"的类型，因为这样的代码复杂而奇怪。
 * 这个驱动程序的问题已经够多了。
 *
 * 'stretch'告诉某些类型是否需要倍道（例如在1.2MB驱动器中的360kB软盘等）。
 * 其他字段应该是不言自明的。
 */
static struct floppy_struct
{
    unsigned int size, sect, head, track, stretch; // 大小、每磁道扇区数、磁头数、磁道数、倍道标志
    unsigned char gap, rate, spec1;                // 间隙、速率、规格参数1
} floppy_type[] = {
    {0, 0, 0, 0, 0, 0x00, 0x00, 0x00},      /* 不测试 */
    {720, 9, 2, 40, 0, 0x2A, 0x02, 0xDF},   /* 360kB PC软盘 */
    {2400, 15, 2, 80, 0, 0x1B, 0x00, 0xDF}, /* 1.2 MB AT软盘 */
    {720, 9, 2, 40, 1, 0x2A, 0x02, 0xDF},   /* 720kB驱动器中的360kB软盘 */
    {1440, 9, 2, 80, 0, 0x2A, 0x02, 0xDF},  /* 3.5" 720kB软盘 */
    {720, 9, 2, 40, 1, 0x23, 0x01, 0xDF},   /* 1.2MB驱动器中的360kB软盘 */
    {1440, 9, 2, 80, 0, 0x23, 0x01, 0xDF},  /* 1.2MB驱动器中的720kB软盘 */
    {2880, 18, 2, 80, 0, 0x1B, 0x00, 0xCF}, /* 1.44MB软盘 */
};
/*
 * 速率：0表示500kb/s，2表示300kbps，1表示250kbps
 * Spec1是0xSH，其中S是步进速率（F=1ms, E=2ms, D=3ms等），
 * H是磁头卸载时间（1=16ms, 2=32ms等）
 *
 * Spec2是(HLD<<1 | ND)，其中HLD是磁头加载时间（1=2ms, 2=4ms等）
 * ND置位表示不使用DMA。硬编码为6（HLD=6ms，使用DMA）。
 */

extern void floppy_interrupt(void); // 软盘中断处理程序
extern char tmp_floppy_area[1024];  // 临时软盘区域，用于DMA缓冲区

/*
 * 这些是全局变量，因为这是向中断提供信息的最简单方式。
 * 它们是当前请求使用的数据。
 */
static int cur_spec1 = -1;              // 当前规格参数1
static int cur_rate = -1;               // 当前速率
static struct floppy_struct *floppy = floppy_type; // 当前软盘类型结构指针
static unsigned char current_drive = 0; // 当前驱动器号
static unsigned char sector = 0;        // 当前扇区号
static unsigned char head = 0;          // 当前磁头号
static unsigned char track = 0;         // 当前磁道号
static unsigned char seek_track = 0;    // 寻道目标磁道
static unsigned char current_track = 255; // 当前磁道（初始化为无效值）
static unsigned char command = 0;       // 命令（FD_READ或FD_WRITE）
unsigned char selected = 0;             // 驱动器选择标志
struct task_struct *wait_on_floppy_select = NULL; // 等待软盘选择的进程

// 取消选择软盘驱动器
void floppy_deselect(unsigned int nr)
{
    if (nr != (current_DOR & 3)) // 检查是否是当前选择的驱动器
        printk("floppy_deselect: drive not selected\n\r");
    selected = 0; // 清除选择标志
    wake_up(&wait_on_floppy_select); // 唤醒等待的进程
}

/*
 * floppy-change永远不会从中断调用，所以我们可以稍微放松一下，
 * 睡眠等。注意floppy-on尝试设置current_DOR指向所需的驱动器，
 * 但如果同时使用多个软盘，它可能无法在睡眠中存活：因此使用循环。
 */
int floppy_change(unsigned int nr) // 检查软盘是否更换
{
repeat:
    floppy_on(nr); // 打开软盘驱动器马达
    while ((current_DOR & 3) != nr && selected) // 等待驱动器被选择
        interruptible_sleep_on(&wait_on_floppy_select);
    if ((current_DOR & 3) != nr) // 如果不是所需的驱动器，重试
        goto repeat;
    if (inb(FD_DIR) & 0x80) // 检查磁盘更换线状态
    {
        floppy_off(nr); // 关闭马达
        return 1;       // 返回已更换
    }
    floppy_off(nr);     // 关闭马达
    return 0;           // 返回未更换
}

// 复制缓冲区宏（从from复制到to，每次4字节）
#define copy_buffer(from, to) \
    __asm__("cld ; rep ; movsl" ::"c"(BLOCK_SIZE / 4), "S"((long)(from)), "D"((long)(to)))

// 设置DMA通道进行数据传输
static void setup_DMA(void)
{
    long addr = (long)CURRENT->buffer; // 获取当前请求的缓冲区地址

    cli(); // 关中断
    if (addr >= 0x100000) // 如果地址高于1MB（在DMA范围之外）
    {
        addr = (long)tmp_floppy_area; // 使用临时缓冲区
        if (command == FD_WRITE)      // 如果是写操作
            copy_buffer(CURRENT->buffer, tmp_floppy_area); // 复制数据到临时缓冲区
    }
    /* 屏蔽DMA通道2 */
    immoutb_p(4 | 2, 10);
    /* 输出命令字节。我不知道为什么，但每个人（minix, sanches & canton）都输出两次，先到12然后到11 */
    __asm__("outb %%al,$12\n\tjmp 1f\n1:\tjmp 1f\n1:\t"
            "outb %%al,$11\n\tjmp 1f\n1:\tjmp 1f\n1:" ::
                "a"((char)((command == FD_READ) ? DMA_READ : DMA_WRITE)));
    /* 地址的低8位 */
    immoutb_p(addr, 4);
    addr >>= 8;
    /* 地址的8-15位 */
    immoutb_p(addr, 4);
    addr >>= 8;
    /* 地址的16-19位 */
    immoutb_p(addr, 0x81);
    /* 计数-1的低8位（1024-1=0x3ff） */
    immoutb_p(0xff, 5);
    /* 计数-1的高8位 */
    immoutb_p(3, 5);
    /* 激活DMA通道2 */
    immoutb_p(0 | 2, 10);
    sti(); // 开中断
}

// 向软盘控制器输出一个字节
static void output_byte(char byte)
{
    int counter;
    unsigned char status;

    if (reset) // 如果需要复位，直接返回
        return;
    for (counter = 0; counter < 10000; counter++) // 尝试10000次
    {
        status = inb_p(FD_STATUS) & (STATUS_READY | STATUS_DIR); // 读取状态
        if (status == STATUS_READY) // 如果控制器就绪且方向正确
        {
            outb(byte, FD_DATA); // 输出字节
            return;
        }
    }
    reset = 1; // 设置复位标志
    printk("Unable to send byte to FDC\n\r"); // 输出错误信息
}

// 从软盘控制器读取结果
static int result(void)
{
    int i = 0, counter, status;

    if (reset) // 如果需要复位，返回错误
        return -1;
    for (counter = 0; counter < 10000; counter++) // 尝试10000次
    {
        status = inb_p(FD_STATUS) & (STATUS_DIR | STATUS_READY | STATUS_BUSY); // 读取状态
        if (status == STATUS_READY) // 如果就绪，返回结果数量
            return i;
        if (status == (STATUS_DIR | STATUS_READY | STATUS_BUSY)) // 如果有数据可读
        {
            if (i >= MAX_REPLIES) // 如果结果太多，跳出
                break;
            reply_buffer[i++] = inb_p(FD_DATA); // 读取结果字节
        }
    }
    reset = 1; // 设置复位标志
    printk("Getstatus times out\n\r"); // 输出错误信息
    return -1;
}

// 处理错误的软盘中断
static void bad_flp_intr(void)
{
    CURRENT->errors++; // 增加错误计数
    if (CURRENT->errors > MAX_ERRORS) // 如果超过最大错误数
    {
        floppy_deselect(current_drive); // 取消选择驱动器
        end_request(0);                 // 结束请求（失败）
    }
    if (CURRENT->errors > MAX_ERRORS / 2) // 如果错误较多
        reset = 1;      // 需要复位控制器
    else
        recalibrate = 1; // 否则只需重新校准
}

/*
 * 这个中断在DMA读/写成功后调用，
 * 所以我们检查结果，并复制任何缓冲区。
 */
static void rw_interrupt(void) // 读写中断处理程序
{
    if (result() != 7 || (ST0 & 0xf8) || (ST1 & 0xbf) || (ST2 & 0x73)) // 检查结果是否有效
    {
        if (ST1 & 0x02) // 如果是写保护错误
        {
            printk("Drive %d is write protected\n\r", current_drive); // 输出写保护信息
            floppy_deselect(current_drive); // 取消选择驱动器
            end_request(0);                 // 结束请求（失败）
        }
        else
            bad_flp_intr(); // 处理其他错误
        do_fd_request();    // 处理下一个请求
        return;
    }
    if (command == FD_READ && (unsigned long)(CURRENT->buffer) >= 0x100000) // 如果是读操作且缓冲区在高地址
        copy_buffer(tmp_floppy_area, CURRENT->buffer); // 从临时缓冲区复制数据
    floppy_deselect(current_drive); // 取消选择驱动器
    end_request(1);                 // 结束请求（成功）
    do_fd_request();                // 处理下一个请求
}

// 设置软盘读写操作
static inline void setup_rw_floppy(void)
{
    setup_DMA(); // 设置DMA
    do_floppy = rw_interrupt; // 设置中断处理程序为rw_interrupt
    output_byte(command);     // 输出命令
    output_byte(head << 2 | current_drive); // 输出磁头和驱动器号
    output_byte(track);       // 输出磁道号
    output_byte(head);        // 输出磁头号
    output_byte(sector);      // 输出扇区号
    output_byte(2);           /* 扇区大小 = 512 */
    output_byte(floppy->sect); // 输出每磁道扇区数
    output_byte(floppy->gap);  // 输出间隙参数
    output_byte(0xFF);        /* 扇区大小（当n!=0时为0xff？） */
    if (reset)                // 如果需要复位
        do_fd_request();      // 处理下一个请求
}

/*
 * 这是在每次寻道（或重新校准）中断后调用的例程。
 * 注意"意外中断"例程也会重新校准，但不会来到这里。
 */
static void seek_interrupt(void) // 寻道中断处理程序
{
    /* 检测驱动器状态 */
    output_byte(FD_SENSEI);
    if (result() != 2 || (ST0 & 0xF8) != 0x20 || ST1 != seek_track) // 检查结果
    {
        bad_flp_intr();    // 处理错误
        do_fd_request();   // 处理下一个请求
        return;
    }
    current_track = ST1;   // 更新当前磁道
    setup_rw_floppy();     // 设置读写操作
}

/*
 * 当传输的一切都应该正确设置时调用这个例程
 * （即软盘马达已打开并且选择了正确的软盘）。
 */
static void transfer(void) // 传输函数
{
    if (cur_spec1 != floppy->spec1) // 如果规格参数改变
    {
        cur_spec1 = floppy->spec1; // 更新当前规格参数
        output_byte(FD_SPECIFY);   // 输出指定命令
        output_byte(cur_spec1);    /* 磁头卸载时间等 */
        output_byte(6);            /* 磁头加载时间=6ms，DMA */
    }
    if (cur_rate != floppy->rate)  // 如果速率改变
        outb_p(cur_rate = floppy->rate, FD_DCR); // 更新数据控制寄存器
    if (reset) // 如果需要复位
    {
        do_fd_request(); // 处理下一个请求
        return;
    }
    if (!seek) // 如果不需要寻道
    {
        setup_rw_floppy(); // 直接设置读写操作
        return;
    }
    do_floppy = seek_interrupt; // 设置中断处理程序为寻道中断
    if (seek_track) // 如果需要寻道到特定磁道
    {
        output_byte(FD_SEEK);              // 输出寻道命令
        output_byte(head << 2 | current_drive); // 输出磁头和驱动器号
        output_byte(seek_track);           // 输出目标磁道
    }
    else // 否则重新校准
    {
        output_byte(FD_RECALIBRATE);       // 输出重新校准命令
        output_byte(head << 2 | current_drive); // 输出磁头和驱动器号
    }
    if (reset) // 如果需要复位
        do_fd_request(); // 处理下一个请求
}

/*
 * 特殊情况 - 在意外中断（或复位）后使用
 */
static void recal_interrupt(void) // 重新校准中断处理程序
{
    output_byte(FD_SENSEI); // 检测中断状态
    if (result() != 2 || (ST0 & 0xE0) == 0x60) // 检查结果
        reset = 1;      // 需要复位
    else
        recalibrate = 0; // 清除重新校准标志
    do_fd_request();    // 处理下一个请求
}

// 意外软盘中断处理程序
void unexpected_floppy_interrupt(void)
{
    output_byte(FD_SENSEI); // 检测中断状态
    if (result() != 2 || (ST0 & 0xE0) == 0x60) // 检查结果
        reset = 1;      // 需要复位
    else
        recalibrate = 1; // 需要重新校准
}

// 重新校准软盘
static void recalibrate_floppy(void)
{
    recalibrate = 0;    // 清除重新校准标志
    current_track = 0;  // 重置当前磁道
    do_floppy = recal_interrupt; // 设置中断处理程序
    output_byte(FD_RECALIBRATE); // 输出重新校准命令
    output_byte(head << 2 | current_drive); // 输出磁头和驱动器号
    if (reset) // 如果需要复位
        do_fd_request(); // 处理下一个请求
}

// 复位中断处理程序
static void reset_interrupt(void)
{
    output_byte(FD_SENSEI); // 检测中断状态
    (void)result();         // 读取结果（忽略）
    output_byte(FD_SPECIFY); // 输出指定命令
    output_byte(cur_spec1);  /* 磁头卸载时间等 */
    output_byte(6);          /* 磁头加载时间=6ms，DMA */
    do_fd_request();         // 处理下一个请求
}

/*
 * 通过将DOR的位2拉低一段时间来复位。
 */
static void reset_floppy(void) // 复位软盘控制器
{
    int i;

    reset = 0;         // 清除复位标志
    cur_spec1 = -1;    // 重置规格参数
    cur_rate = -1;     // 重置速率
    recalibrate = 1;   // 需要重新校准
    printk("Reset-floppy called\n\r"); // 输出信息
    cli();             // 关中断
    do_floppy = reset_interrupt; // 设置中断处理程序
    outb_p(current_DOR & ~0x04, FD_DOR); // 拉低复位线
    for (i = 0; i < 100; i++) // 延迟
        __asm__("nop");
    outb(current_DOR, FD_DOR); // 恢复DOR
    sti();             // 开中断
}

// 软盘开启中断处理程序
static void floppy_on_interrupt(void)
{
    /* 我们不能进行软盘选择，因为这可能会睡眠。我们只是强制它 */
    selected = 1; // 设置选择标志
    if (current_drive != (current_DOR & 3)) // 如果当前驱动器不是所需的
    {
        current_DOR &= 0xFC;          // 清除驱动器位
        current_DOR |= current_drive; // 设置新驱动器
        outb(current_DOR, FD_DOR);    // 输出到数字输出寄存器
        add_timer(2, &transfer);      // 添加定时器以稍后调用传输
    }
    else
        transfer(); // 否则立即传输
}

// 软盘请求处理函数
void do_fd_request(void)
{
    unsigned int block;

    seek = 0; // 清除寻道标志
    if (reset) // 如果需要复位
    {
        reset_floppy(); // 复位软盘控制器
        return;
    }
    if (recalibrate) // 如果需要重新校准
    {
        recalibrate_floppy(); // 重新校准
        return;
    }
    INIT_REQUEST; // 初始化请求（检查当前请求有效性）
    floppy = (MINOR(CURRENT->dev) >> 2) + floppy_type; // 获取软盘类型
    if (current_drive != CURRENT_DEV) // 如果驱动器改变
        seek = 1;        // 需要寻道
    current_drive = CURRENT_DEV; // 设置当前驱动器
    block = CURRENT->sector; // 获取请求的扇区号
    if (block + 2 > floppy->size) // 如果扇区超出范围
    {
        end_request(0); // 结束请求（失败）
        goto repeat;    // 跳转到repeat（在INIT_REQUEST宏中定义）
    }
    sector = block % floppy->sect; // 计算扇区号
    block /= floppy->sect;
    head = block % floppy->head;   // 计算磁头号
    track = block / floppy->head;  // 计算磁道号
    seek_track = track << floppy->stretch; // 计算寻道目标磁道（考虑倍道）
    if (seek_track != current_track) // 如果需要寻道
        seek = 1;        // 设置寻道标志
    sector++;            // 扇区号从1开始（之前计算的是0-based）
    if (CURRENT->cmd == READ) // 如果是读请求
        command = FD_READ;
    else if (CURRENT->cmd == WRITE) // 如果是写请求
        command = FD_WRITE;
    else
        panic("do_fd_request: unknown command"); // 未知命令，恐慌
    add_timer(ticks_to_floppy_on(current_drive), &floppy_on_interrupt); // 添加定时器以开启软盘
}

// 软盘初始化函数
void floppy_init(void)
{
    blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST; // 设置块设备请求函数
    set_trap_gate(0x26, &floppy_interrupt);        // 设置软盘中断门
    outb(inb_p(0x21) & ~0x40, 0x21);               // 允许软盘中断（屏蔽字位6清零）
}