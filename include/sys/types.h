// 总的来说，这个头文件定义了许多在系统编程中常用的数据类型，
// 这些类型在不同系统架构上可能有不同的实际大小，
// 但通过这种抽象可以保证程序的可移植性。

// 这是头文件保护宏，防止该头文件被多次包含。
// _SYS_TYPES_H是一个唯一标识符，确保内容只被编译一次。

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

// 定义size_t类型，这是一个无符号整数类型，通常用于表示内存大小或数组索引
#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

// 自纪元以来的秒数，一共可以表示2^64,完全足够
#ifndef _TIME_T
#define _TIME_T
typedef long time_t;
#endif

// 定义ptrdiff_t类型，用于表示两个指针相减的结果
// 得到的是以类型单元为单位的大小
#ifndef _PTRDIFF_T
#define _PTRDIFF_T
typedef long ptrdiff_t;
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif
// 进程id
typedef int pid_t;
// 用户id
typedef unsigned short uid_t;
// 组类型id
typedef unsigned char gid_t;
// 设备号id
typedef unsigned short dev_t;
// inode节点类型
typedef unsigned short ino_t;
// 文件权限和模式类型
typedef unsigned short mode_t;
// 用户模式类型
typedef unsigned short umode_t;
// 硬链接数计数类型
typedef unsigned char nlink_t;
// 磁盘地址类型
typedef int daddr_t;
// 文件偏移量类型
typedef long off_t;
// 无符号字符类型
typedef unsigned char u_char;
// 无符号短整数类型
typedef unsigned short ushort;
// 定义div_t结构体，用于存储整数除法的结果（商quot和余数rem）
typedef struct
{
    int quot, rem;
} div_t;

// 定义ldiv_t结构体，用于存储长整数除法的结果
typedef struct
{
    long quot, rem;
} ldiv_t;

// 定义ustat结构体，用于存储文件系统的统计信息
struct ustat
{
    daddr_t f_tfree; // 空闲磁盘块数量
    ino_t f_tinode;  // 空闲i节点数量
    char f_fname[6]; // 文件系统名称
    char f_fpack[6]; // 文件系统.pack名称
};
#endif
