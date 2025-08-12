/*
 *  linux/kernel/printk.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * When in kernel-mode, we cannot use printf, as fs is liable to
 * point to 'interesting' things. Make a printf with fs-saving, and
 * all is well.
 */
#include <stdarg.h>
#include <stddef.h>

#include <linux/kernel.h>

static char buf[1024];

/**
 * @brief 格式化字符串函数，并且返回格式化后的字符串长度
 *
 * @param buf 输出缓冲区
 * @param fmt 格式化字符串
 * @param args 可变参数列表
 * @return int 输出字符串的长度
 */

extern int vsprintf(char *buf, const char *fmt, va_list args);

/**
 * @brief 打印内核消息函数
 *
 * @param fmt 格式化字符串
 * @return int 输出字符串的长度
 * 注意：printk负责格式化，tty_write负责硬件相关的实际写入，职责分离使代码更易维护。
 */
int printk(const char *fmt, ...)
{
    va_list args; /*可变参数列表指针*/
    int i;
    /*初始化可变参数列表指针*/
    va_start(args, fmt);
    /*格式化字符串长度*/
    i = vsprintf(buf, fmt, args);
    /*结束可变参数列表指针*/
    va_end(args);
    /*使用 GCC 内联汇编直接操作硬件和调用内核函数tty_write输出内容*/
    __asm__("push %%fs\n\t"      /*保存fs寄存器*/
            "push %%ds\n\t"      /*保存ds寄存器*/
            "pop %%fs\n\t"       /*恢复fs寄存器*/
            "pushl %0\n\t"       /*压入字符串长度i作为参数*/
            "pushl $buf\n\t"     /*压入缓冲区地址作为参数*/
            "pushl $0\n\t"       /*压入文件描述符0(通常代表控制台)*/
            "call tty_write\n\t" /*调用tty_write函数*/
            "addl $8,%%esp\n\t"  /*清理栈上的两个参数，返回值依旧保留的*/
            "popl %0\n\t"        /*恢复i的值*/
            "pop %%fs\n\t"       /*恢复fs寄存器*/
            : /*没有实际的输出寄存器*/
            : "r"(i) /*输入：将i放入寄存器.注意这里没有输出*/
            : "ax", "cx", "dx");/*告知编译器这些寄存器(ax cs dx)会被修改*/
    return i;
}
