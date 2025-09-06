/*
 *  linux/fs/char_dev.c
 *
 *  (C) 1991  Linus Torvalds
 *
 *  功能说明：Linux 0.11 内核中字符设备（Character Device）的核心管理与读写调度实现
 *  字符设备是按字节流顺序访问的设备（如终端、串口、键盘、打印机等），
 *  此文件通过“设备类型表+函数指针”机制，统一调度不同字符设备的读写操作，
 *  是内核实现“一切皆文件”理念中字符设备抽象的关键模块
 */
#include <errno.h>     // 定义错误码（如ENODEV、EPERM、EIO）
#include <sys/types.h> // 定义基本数据类型（如off_t偏移量类型）

#include <linux/sched.h>  // 进程调度相关结构（如current当前进程指针、tty终端号）
#include <linux/kernel.h> // 内核核心函数/宏定义（无直接调用，为编译依赖）

#include <asm/segment.h> // 段操作函数（put_fs_byte/get_fs_byte，实现用户/内核空间数据传输）
#include <asm/io.h>      // IO端口操作函数（inb/outb，用于读写硬件IO端口）
// 外部声明：终端设备的读写函数（实际实现位于tty相关文件，此处仅调用）
extern int tty_read(unsigned minor, char *buf, int count);
extern int tty_write(unsigned minor, char *buf, int count);
// 定义字符设备读写函数指针类型：统一不同字符设备的读写接口
// 参数说明：
// rw：读写标识（READ=0表示读，WRITE=1表示写）
// minor：次设备号（区分同一主设备下的不同子设备）
// buf：用户空间数据缓冲区（读时存结果，写时存待写数据）
// count：期望读写的字节数
// pos：文件读写偏移量（字符设备多不依赖偏移，部分场景如/dev/mem需使用）
typedef int (*crw_ptr)(int rw, unsigned minor, char *buf, int count, off_t *pos);
/**
 * @brief 特定终端设备（/dev/ttyx，x为0/1/2等）的读写调度函数
 * @param rw 读写标识（READ/WRITE）
 * @param minor 次设备号（对应具体的tty终端，如minor=0对应/dev/tty0）
 * @param buf 用户空间数据缓冲区
 * @param count 期望读写的字节数
 * @param pos 读写偏移量（终端设备忽略，仅为适配函数指针类型）
 * @return 实际读写的字节数；失败返回负错误码
 */
static int rw_ttyx(int rw, unsigned minor, char *buf, int count, off_t *pos)
{
    // 根据读写标识，调用对应的终端读写函数
    return ((rw == READ) ? tty_read(minor, buf, count) : tty_write(minor, buf, count));
}
/**
 * @brief 当前进程关联终端（/dev/tty）的读写调度函数
 * @param rw 读写标识（READ/WRITE）
 * @param minor 次设备号（未使用，因当前进程终端由current->tty确定）
 * @param buf 用户空间数据缓冲区
 * @param count 期望读写的字节数
 * @param pos 读写偏移量（终端设备忽略）
 * @return 实际读写的字节数；失败返回负错误码
 */
static int rw_tty(int rw, unsigned minor, char *buf, int count, off_t *pos)
{
    // 检查当前进程是否关联终端（current->tty=-1表示无关联终端）
    if (current->tty < 0)
        return -EPERM; // 无权限错误（Operation not permitted）

    // 调用rw_ttyx，使用当前进程的终端号作为次设备号
    return rw_ttyx(rw, current->tty, buf, count, pos);
}
/**
 * @brief 物理内存设备（/dev/ram，模拟内存盘）的读写函数（占位实现）
 * @note 此版本未实现实际功能，仅返回IO错误，用于后续扩展
 */
static int rw_ram(int rw, char *buf, int count, off_t *pos)
{
    return -EIO; // IO错误（Input/output error）
}
/**
 * @brief 物理内存映射设备（/dev/mem，直接访问物理内存）的读写函数（占位实现）
 * @note 未实现实际功能，仅返回IO错误，用于后续扩展
 */
static int rw_mem(int rw, char *buf, int count, off_t *pos)
{
    return -EIO;
}
/**
 * @brief 内核内存设备（/dev/kmem，访问内核虚拟内存）的读写函数（占位实现）
 * @note 未实现实际功能，仅返回IO错误，用于后续扩展
 */
static int rw_kmem(int rw, char *buf, int count, off_t *pos)
{
    return -EIO;
}
/**
 * @brief IO端口设备（/dev/port，直接访问硬件IO端口）的读写函数
 * @param rw 读写标识（READ/WRITE）
 * @param buf 用户空间数据缓冲区
 * @param count 期望读写的字节数
 * @param pos IO端口起始地址（偏移量即端口号，如pos=0x3F8对应串口1端口）
 * @return 实际读写的端口字节数；失败返回负错误码（此处无显式失败，因端口访问不返回错误）
 */
static int rw_port(int rw, char *buf, int count, off_t *pos)
{
    int i = *pos; // 记录起始IO端口号

    // 循环读写每个IO端口：限制端口号在0~65535（16位IO端口地址空间）
    while (count-- > 0 && i < 65536)
    {
        if (rw == READ)
        {
            // 读IO端口：inb(i)读取端口i的字节，通过put_fs_byte写入用户缓冲区
            put_fs_byte(inb(i), buf++);
        }
        else
        {
            // 写IO端口：get_fs_byte从用户缓冲区读字节，通过outb写入端口i
            outb(get_fs_byte(buf++), i);
        }
        i++; // 端口号递增（按字节访问）
    }

    i -= *pos; // 计算实际读写的端口字节数
    *pos += i; // 更新偏移量（下次从下一个端口开始）
    return i;  // 返回实际读写的字节数
}

/**
 * @brief 内存类字符设备（/dev/mem、/dev/kmem、/dev/port等）的读写调度函数
 * @param rw 读写标识（READ/WRITE）
 * @param minor 次设备号（区分不同内存类设备，如minor=4对应/dev/port）
 * @param buf 用户空间数据缓冲区
 * @param count 期望读写的字节数
 * @param pos 读写偏移量（内存地址或IO端口号）
 * @return 实际读写的字节数；失败返回负错误码
 */
static int rw_memory(int rw, unsigned minor, char *buf, int count, off_t *pos)
{
    // 根据次设备号，调度到对应设备的读写逻辑
    switch (minor)
    {
    case 0:
        return rw_ram(rw, buf, count, pos); // minor=0 → /dev/ram（内存盘）
    case 1:
        return rw_mem(rw, buf, count, pos); // minor=1 → /dev/mem（物理内存）
    case 2:
        return rw_kmem(rw, buf, count, pos); // minor=2 → /dev/kmem（内核内存）
    case 3:
        // minor=3 → /dev/null（空设备）：读返回0（无数据），写返回请求字节数（数据丢弃）
        return (rw == READ) ? 0 : count;
    case 4:
        return rw_port(rw, buf, count, pos); // minor=4 → /dev/port（IO端口）
    default:
        return -EIO; // 未知次设备号，返回IO错误
    }
}

// 计算字符设备类型表的长度（主设备号最大范围）
// crw_table数组下标对应“主设备号”，元素为该主设备的读写调度函数指针
#define NRDEVS ((sizeof(crw_table)) / (sizeof(crw_ptr)))
/**
 * 字符设备读写调度表（核心映射表）
 * 下标 = 主设备号（Major Device Number），元素 = 该主设备的读写调度函数
 * 主设备号含义说明（Linux 0.11 约定）：
 * 0：无设备（nodev）
 * 1：内存类设备（/dev/mem、/dev/kmem、/dev/port等）
 * 2：软盘设备（/dev/fd，此处未实现，为NULL）
 * 3：硬盘设备（/dev/hd，字符设备接口未实现，为NULL）
 * 4：特定终端设备（/dev/tty0、/dev/tty1等）
 * 5：当前进程终端设备（/dev/tty）
 * 6：打印机设备（/dev/lp，未实现，为NULL）
 * 7：匿名管道（unnamed pipes，管道实际由pipe.c实现，此处占位为NULL）
 */
static crw_ptr crw_table[] = {
    NULL,      /* 0: nodev（无设备） */
    rw_memory, /* 1: /dev/mem、/dev/kmem、/dev/port等内存类设备 */
    NULL,      /* 2: /dev/fd（软盘设备，未实现） */
    NULL,      /* 3: /dev/hd（硬盘设备，字符接口未实现） */
    rw_ttyx,   /* 4: /dev/ttyx（特定终端设备） */
    rw_tty,    /* 5: /dev/tty（当前进程终端） */
    NULL,      /* 6: /dev/lp（打印机设备，未实现） */
    NULL       /* 7: unnamed pipes（匿名管道，由pipe.c实现） */
};
/**
 * @brief 字符设备读写的统一入口函数（系统调用最终触发此函数）
 * @param rw 读写标识（READ=0/WRITE=1）
 * @param dev 设备号（高8位为主设备号，低8位为次设备号）
 * @param buf 用户空间数据缓冲区
 * @param count 期望读写的字节数
 * @param pos 读写偏移量（部分设备如/dev/mem/port需使用）
 * @return 实际读写的字节数；失败返回负错误码（如-ENODEV表示设备不存在）
 */
int rw_char(int rw, int dev, char *buf, int count, off_t *pos)
{
    crw_ptr call_addr; // 指向当前设备的读写调度函数
    // 1. 检查主设备号合法性：主设备号不能超过设备表长度（NRDEVS）
    if (MAJOR(dev) >= NRDEVS)
        return -ENODEV; // 设备不存在错误（No such device）
    // 2. 从调度表中获取当前主设备的读写函数指针
    if (!(call_addr = crw_table[MAJOR(dev)]))
        return -ENODEV; // 该主设备无对应的读写函数（设备未实现）
    // 3. 调用调度函数，传入次设备号及其他参数，返回执行结果
    return call_addr(rw, MINOR(dev), buf, count, pos);
}