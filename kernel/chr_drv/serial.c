/*
 *  linux/kernel/serial.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *	serial.c
 *
 * 该模块实现RS232串口的I/O功能，包括：
 *	void rs_write(struct tty_struct * queue);  // 串口写操作
 *	void rs_init(void);                  // 串口初始化
 * 以及所有与串口I/O相关的中断处理
 */

#include <linux/tty.h>   // 包含终端设备相关的数据结构定义
#include <linux/sched.h> // 包含进程调度相关的定义
#include <asm/system.h>  // 包含系统操作函数，如cli()、sti()等
#include <asm/io.h>      // 包含I/O端口操作函数，如inb()、outb()等

// 当写队列中的数据量低于此值时，唤醒等待的写进程
#define WAKEUP_CHARS (TTY_BUF_SIZE / 4)

// 声明串口中断处理函数（在汇编文件中实现）
extern void rs1_interrupt(void); // 第一个串口的中断处理函数
extern void rs2_interrupt(void); // 第二个串口的中断处理函数

/*
 * 初始化指定端口的串口控制器
 * @param port: 串口控制器的基地址
 */
static void init(int port)
{
    // 设置线路控制寄存器（LCR）的DLAB位（位7），允许设置波特率除数
    outb_p(0x80, port + 3); /* 端口+3是线路控制寄存器，0x80表示设置DLAB */
    // 设置波特率除数的低8位（48表示2400bps，计算公式：115200 / 2400 = 48）
    outb_p(0x30, port); /* 端口是除数寄存器低8位，0x30对应48 */
    // 设置波特率除数的高8位（此处为0）
    outb_p(0x00, port + 1); /* 端口+1是除数寄存器高8位 */
    // 重置DLAB位，同时设置数据格式：8位数据位，1位停止位，无奇偶校验
    outb_p(0x03, port + 3); /* 0x03 = 00000011，DLAB=0，8位数据 */
    // 设置调制解调器控制寄存器（MCR）：使能DTR、RTS和OUT2信号
    // DTR：数据终端就绪，RTS：请求发送，OUT2：用于控制中断
    outb_p(0x0b, port + 4); /* 0x0b = 00001011，DTR=1, RTS=1, OUT2=1 */
    // 设置中断允许寄存器（IER）：允许接收数据就绪、接收线路状态中断
    outb_p(0x0d, port + 1); /* 0x0d = 00001101，使能接收和线路状态中断 */
    // 读取数据端口以复位状态（清除可能的残留数据或中断标志）
    (void)inb(port); /* 读取数据端口（端口+0），忽略返回值 */
}

/*
 * 初始化串口设备和中断处理
 */
void rs_init(void)
{
    // 设置中断门：将IRQ4（对应中断向量0x24）与第一个串口中断处理函数关联
    set_intr_gate(0x24, rs1_interrupt);
    // 设置中断门：将IRQ3（对应中断向量0x23）与第二个串口中断处理函数关联
    set_intr_gate(0x23, rs2_interrupt);
    // 初始化第一个串口（tty1），其读队列的data字段存储串口基地址
    init(tty_table[1].read_q.data);
    // 初始化第二个串口（tty2）
    init(tty_table[2].read_q.data);
    // 允许主8259A中断控制器的IRQ3和IRQ4中断（对应串口2和串口1）
    // 0xE7 = 11100111，清除位3和位4（允许IRQ3和IRQ4）
    outb(inb_p(0x21) & 0xE7, 0x21);
}

/*
 * 当tty_write向写队列中放入数据后，调用此函数
 * 它需要检查队列是否非空，并相应地设置中断寄存器以允许发送中断
 *
 *	void _rs_write(struct tty_struct * tty);
 */
void rs_write(struct tty_struct *tty)
{
    cli(); // 关中断，防止操作过程中被中断干扰
    // 如果写队列非空，设置中断允许寄存器（IER）的发送保持寄存器空中断位
    if (!EMPTY(tty->write_q))
        // 端口+1是中断允许寄存器，0x02表示允许发送保持寄存器空中断
        outb(inb_p(tty->write_q.data + 1) | 0x02, tty->write_q.data + 1);
    sti(); // 开中断
}
