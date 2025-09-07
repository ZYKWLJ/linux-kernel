/*
 *  linux/lib/_exit.c
 *
 *  (C) 1991  Linus Torvalds  // 版权声明，作者为Linus Torvalds
 */

// 定义__LIBRARY__宏，用于启用unistd.h中的系统调用编号定义
#define __LIBRARY__
// 包含unistd.h头文件，该文件声明了系统调用和相关常量
#include <unistd.h>

/*
 * _exit函数：立即终止当前进程
 * 参数：exit_code - 进程退出状态码
 * 注意：与标准库的exit()不同，_exit()不会执行I/O缓冲区刷新或调用atexit()注册的函数
 */
void _exit(int exit_code)
{
    // 内嵌汇编实现系统调用
    // int $0x80是x86架构下触发系统调用的中断指令
    // "a"(__NR_exit)：将系统调用号(__NR_exit)放入eax寄存器
    // "b"(exit_code)：将退出码放入ebx寄存器
    // 这是Linux早期的系统调用调用约定
    __asm__ __volatile__("int $0x80" ::"a"(__NR_exit), "b"(exit_code));
}