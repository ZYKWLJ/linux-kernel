#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

#include <sys/types.h>
/*
 * 系统标识信息结构体：用于存储操作系统的关键标识信息
 * 所有字段均为长度9的字符数组（包含字符串结束符'\0'，实际可存储8个有效字符）
 */
struct utsname
{
    char sysname[9];  /* 操作系统名称（如"Linux"） */
    char nodename[9]; /* 网络节点主机名（当前计算机的主机名） */
    char release[9];  /* 操作系统内核版本号（如"5.4.0"） */
    char version[9];  /* 内核版本详细信息（如编译时间、编译参数等） */
    char machine[9];  /* 硬件架构类型（如"x86_64"、"armv7l"） */
};

/*
 * 系统信息获取函数声明
 * 参数：utsbuf - 指向struct utsname类型的指针，用于接收系统信息
 * 返回值：int类型，成功返回非负数，失败返回-1（并设置errno表示错误原因）
 * 功能：获取当前系统的标识信息（如操作系统名称、内核版本等），并写入utsbuf指向的结构体
 */
extern int uname(struct utsname *utsbuf);

/* 结束头文件保护宏 */
#endif