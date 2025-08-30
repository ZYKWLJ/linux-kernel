/*
 *  linux/kernel/chr_drv/tty_ioctl.c
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>   // 错误号定义头文件
#include <termios.h> // 终端输入输出函数头文件

#include <linux/sched.h>  // 内核调度程序头文件，定义了任务结构 task_struct
#include <linux/kernel.h> // 内核头文件，含有一些内核常用函数的原形定义
#include <linux/tty.h>    // tty 头文件，定义了有关 tty_io，串行通信方面的参数、常数

#include <asm/io.h>      // io 头文件，定义硬件端口输入/输出宏汇编语句
#include <asm/segment.h> // 段操作头文件，定义了有关段寄存器操作的嵌入式汇编函数
#include <asm/system.h>  // 系统头文件，定义了设置或修改描述符/中断门等的嵌入式汇编宏

// 定义波特率因子数组。波特率 = 115200 / quotient[]
static unsigned short quotient[] = {
    0, 2304, 1536, 1047, 857, // 对应波特率：0,50,75,110,134.5
    768, 576, 384, 192, 96,   // 对应波特率：150,200,300,600,1200
    64, 48, 24, 12, 6, 3      // 对应波特率：1800,2400,4800,9600,19200,38400
};

// 改变串行端口传输波特率
static void change_speed(struct tty_struct *tty)
{
    unsigned short port, quot;

    // 如果 tty 读队列数据的端口号是 0，则返回（没有设置端口）
    if (!(port = tty->read_q.data))
        return;
    // 从终端 termios 结构控制标志中取得波特率索引值，并据此从 quotient 数组取得对应的波特率因子值
    quot = quotient[tty->termios.c_cflag & CBAUD];
    cli();                       // 关中断
    outb_p(0x80, port + 3);      // 设置线路控制寄存器 DLAB 位（置位）
    outb_p(quot & 0xff, port);   // 写入波特率因子低字节
    outb_p(quot >> 8, port + 1); // 写入波特率因子高字节
    outb(0x03, port + 3);        // 复位 DLAB 位，并设置数据位为 8 位
    sti();                       // 开中断
}

// 刷新（清空）指定 tty 队列中的字符
static void flush(struct tty_queue *queue)
{
    cli();                     // 关中断
    queue->head = queue->tail; // 让队列头指针等于尾指针，从而清空队列
    sti();                     // 开中断
}

// 等待直到发送完所有字符（未实现）
static void wait_until_sent(struct tty_struct *tty)
{
    /* do nothing - not implemented */
}

// 发送 BREAK 控制字符（未实现）
static void send_break(struct tty_struct *tty)
{
    /* do nothing - not implemented */
}

// 获取终端 termios 结构信息
static int get_termios(struct tty_struct *tty, struct termios *termios)
{
    int i;

    // 验证用户内存空间 termios 处有足够空间存放 termios 结构
    verify_area(termios, sizeof(*termios));
    // 逐字节复制 tty 终端 termios 结构数据到用户空间 termios 处
    for (i = 0; i < (sizeof(*termios)); i++)
        put_fs_byte(((char *)&tty->termios)[i], i + (char *)termios);
    return 0;
}

// 设置终端 termios 结构信息
static int set_termios(struct tty_struct *tty, struct termios *termios)
{
    int i;

    // 逐字节从用户空间 termios 处复制数据到 tty 终端 termios 结构中
    for (i = 0; i < (sizeof(*termios)); i++)
        ((char *)&tty->termios)[i] = get_fs_byte(i + (char *)termios);
    change_speed(tty); // 改变传输波特率
    return 0;
}

// 获取终端 termio 结构信息（较老的终端接口）
static int get_termio(struct tty_struct *tty, struct termio *termio)
{
    int i;
    struct termio tmp_termio; // 临时 termio 结构

    // 验证用户内存空间 termio 处有足够空间
    verify_area(termio, sizeof(*termio));
    // 将 tty 终端 termios 结构中的信息复制到临时 termio 结构中
    tmp_termio.c_iflag = tty->termios.c_iflag;
    tmp_termio.c_oflag = tty->termios.c_oflag;
    tmp_termio.c_cflag = tty->termios.c_cflag;
    tmp_termio.c_lflag = tty->termios.c_lflag;
    tmp_termio.c_line = tty->termios.c_line;
    for (i = 0; i < NCC; i++)
        tmp_termio.c_cc[i] = tty->termios.c_cc[i];
    // 将临时 termio 结构中的数据复制到用户空间 termio 处
    for (i = 0; i < (sizeof(*termio)); i++)
        put_fs_byte(((char *)&tmp_termio)[i], i + (char *)termio);
    return 0;
}

/*
 * This only works as the 386 is low-byte-first （仅适用于 386 小字节序）
 */
// 设置终端 termio 结构信息（较老的终端接口）
static int set_termio(struct tty_struct *tty, struct termio *termio)
{
    int i;
    struct termio tmp_termio; // 临时 termio 结构

    // 逐字节从用户空间 termio 处复制数据到临时 termio 结构中
    for (i = 0; i < (sizeof(*termio)); i++)
        ((char *)&tmp_termio)[i] = get_fs_byte(i + (char *)termio);
    // 将临时 termio 结构中的信息复制到 tty 终端 termios 结构中
    *(unsigned short *)&tty->termios.c_iflag = tmp_termio.c_iflag;
    *(unsigned short *)&tty->termios.c_oflag = tmp_termio.c_oflag;
    *(unsigned short *)&tty->termios.c_cflag = tmp_termio.c_cflag;
    *(unsigned short *)&tty->termios.c_lflag = tmp_termio.c_lflag;
    tty->termios.c_line = tmp_termio.c_line;
    for (i = 0; i < NCC; i++)
        tty->termios.c_cc[i] = tmp_termio.c_cc[i];
    change_speed(tty); // 改变传输波特率
    return 0;
}

// tty 输入输出控制函数
int tty_ioctl(int dev, int cmd, int arg)
{
    struct tty_struct *tty;

    // 如果设备主设备号是 5（控制终端），则取当前进程的控制终端号
    if (MAJOR(dev) == 5)
    {
        dev = current->tty;
        if (dev < 0)
            panic("tty_ioctl: dev<0");
    }
    else
        dev = MINOR(dev);  // 否则取次设备号
    tty = dev + tty_table; // 获取 tty 结构指针（tty_table 是 tty 结构数组）

    // 根据命令 cmd 进行分支处理
    switch (cmd)
    {
    case TCGETS: // 取终端 termios 结构
        return get_termios(tty, (struct termios *)arg);
    case TCSETSF:                               // 先刷新输入队列，然后设置终端 termios 结构
        flush(&tty->read_q); /* fallthrough */  // 向下穿透执行
    case TCSETSW:                               // 等待输出处理完毕，然后设置终端 termios 结构
        wait_until_sent(tty); /* fallthrough */ // 向下穿透执行
    case TCSETS:                                // 设置终端 termios 结构
        return set_termios(tty, (struct termios *)arg);
    case TCGETA: // 取终端 termio 结构
        return get_termio(tty, (struct termio *)arg);
    case TCSETAF:                               // 先刷新输入队列，然后设置终端 termio 结构
        flush(&tty->read_q); /* fallthrough */  // 向下穿透执行
    case TCSETAW:                               // 等待输出处理完毕，然后设置终端 termio 结构
        wait_until_sent(tty); /* fallthrough */ // 向下穿透执行
    case TCSETA:                                // 设置终端 termio 结构
        return set_termio(tty, (struct termio *)arg);
    case TCSBRK: // 发送 break 信号
        if (!arg)
        {
            wait_until_sent(tty); // 等待输出完毕
            send_break(tty);      // 发送 break
        }
        return 0;
    case TCXONC:        // 开始/停止控制（未实现）
        return -EINVAL; /* not implemented */
    case TCFLSH:        // 刷新 tty 队列
        if (arg == 0)   // 刷新输入队列
            flush(&tty->read_q);
        else if (arg == 1) // 刷新输出队列
            flush(&tty->write_q);
        else if (arg == 2)
        { // 刷新输入和输出队列
            flush(&tty->read_q);
            flush(&tty->write_q);
        }
        else
            return -EINVAL; // 无效参数
        return 0;
    case TIOCEXCL:                                    // 设置终端为独占模式（未实现）
        return -EINVAL;                               /* not implemented */
    case TIOCNXCL:                                    // 复位终端独占模式（未实现）
        return -EINVAL;                               /* not implemented */
    case TIOCSCTTY:                                   // 设置控制终端（未实现）
        return -EINVAL;                               /* set controlling term NI */
    case TIOCGPGRP:                                   // 读取终端前台进程组号
        verify_area((void *)arg, 4);                  // 验证用户空间
        put_fs_long(tty->pgrp, (unsigned long *)arg); // 放入用户指定地址
        return 0;
    case TIOCSPGRP:                                    // 设置终端前台进程组号
        tty->pgrp = get_fs_long((unsigned long *)arg); // 从用户指定地址取值
        return 0;
    case TIOCOUTQ: // 返回输出队列中还未送出的字符数
        verify_area((void *)arg, 4);
        put_fs_long(CHARS(tty->write_q), (unsigned long *)arg);
        return 0;
    case TIOCINQ: // 返回辅助队列中还未读取的字符数
        verify_area((void *)arg, 4);
        put_fs_long(CHARS(tty->secondary),
                    (unsigned long *)arg);
        return 0;
    case TIOCSTI:       // 模拟终端输入（未实现）
        return -EINVAL; /* not implemented */
    case TIOCGWINSZ:    // 读取终端窗口大小（未实现）
        return -EINVAL; /* not implemented */
    case TIOCSWINSZ:    // 设置终端窗口大小（未实现）
        return -EINVAL; /* not implemented */
    case TIOCMGET:      // 返回 modem 状态比特位（未实现）
        return -EINVAL; /* not implemented */
    case TIOCMBIS:      // 设置 modem 状态比特位（未实现）
        return -EINVAL; /* not implemented */
    case TIOCMBIC:      // 清除 modem 状态比特位（未实现）
        return -EINVAL; /* not implemented */
    case TIOCMSET:      // 设置 modem 状态（未实现）
        return -EINVAL; /* not implemented */
    case TIOCGSOFTCAR:  // 读取软件载波标志（未实现）
        return -EINVAL; /* not implemented */
    case TIOCSSOFTCAR:  // 设置软件载波标志（未实现）
        return -EINVAL; /* not implemented */
    default:            // 未知命令，返回无效错误
        return -EINVAL;
    }
}