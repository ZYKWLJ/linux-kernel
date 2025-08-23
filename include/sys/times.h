#ifndef _TIMES_H
#define _TIMES_H

/* 包含系统基础数据类型定义头文件，提供time_t等基础类型 */
#include <sys/types.h>

/*
 * 进程时间统计结构体，用于存储进程及其子进程的CPU时间信息
 * 所有时间单位通常为系统时钟滴答数（需结合时钟频率转换为秒）
 */
struct tms
{
    time_t tms_utime;  /* 当前进程在用户态消耗的CPU时间（执行用户代码） */
    time_t tms_stime;  /* 当前进程在内核态消耗的CPU时间（执行系统调用等内核操作） */
    time_t tms_cutime; /* 当前进程所有已终止子进程的用户态CPU时间总和（"c"表示child） */
    time_t tms_cstime; /* 当前进程所有已终止子进程的内核态CPU时间总和 */
};

/*
 * 时间统计函数声明
 * 参数：tp - 指向struct tms的指针，用于存储获取到的时间统计数据
 * 返回值：系统自启动以来的总时钟滴答数（time_t类型）
 * 功能：获取当前进程及其子进程的CPU时间分布，并通过tp参数返回
 */
extern time_t times(struct tms *tp);

#endif /* 结束头文件保护宏 */