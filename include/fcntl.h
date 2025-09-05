#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/types.h>

/*
 * 文件打开和控制相关的标志定义
 * 注意：NOCTTY 和 NDELAY 功能目前尚未实现
 */

/* 访问模式掩码 - 用于提取文件描述符的访问模式 */
#define O_ACCMODE 00003

/* 访问模式标志 */
#define O_RDONLY 00 /* 只读模式打开文件 */
#define O_WRONLY 01 /* 只写模式打开文件 */
#define O_RDWR 02   /* 读写模式打开文件 */

/* 文件创建和状态标志 */
#define O_CREAT 00100       /* 若文件不存在则创建（仅用于 open 函数） */
#define O_EXCL 00200        /* 与 O_CREAT 同时使用，若文件存在则打开失败（仅用于 open 函数） */
#define O_NOCTTY 00400      /* 不将打开的文件作为控制终端（仅用于 open 函数） */
#define O_TRUNC 01000       /* 若文件存在且以可写模式打开，则截断文件长度为0（仅用于 open 函数） */
#define O_APPEND 02000      /* 以追加模式打开，所有写操作都在文件末尾 */
#define O_NONBLOCK 04000    /* 非阻塞模式打开（仅用于 open 函数） */
#define O_NDELAY O_NONBLOCK /* 与 O_NONBLOCK 相同，提供兼容性 */

/*
 * fcntl 函数的命令参数定义
 * 注意：目前不支持文件锁定功能，其他功能也未经过全面测试
 */
#define F_DUPFD 0  /* 复制文件描述符 */
#define F_GETFD 1  /* 获取文件描述符标志 */
#define F_SETFD 2  /* 设置文件描述符标志 */
#define F_GETFL 3  /* 获取文件状态标志（包括 cloexec 等） */
#define F_SETFL 4  /* 设置文件状态标志 */
#define F_GETLK 5  /* 获取文件锁状态（未实现） */
#define F_SETLK 6  /* 设置文件锁（未实现） */
#define F_SETLKW 7 /* 设置文件锁，若无法获取则阻塞等待（未实现） */

/* 用于 F_GETFL 和 F_SETFL 的标志 */
#define FD_CLOEXEC 1 /* 执行 exec 时关闭文件描述符（低位置1即生效） */

/*
 * 文件锁定类型定义
 * 这些功能目前在所有层面都未实现，但为了符合 POSIX 标准而定义
 */
#define F_RDLCK 0 /* 共享读锁 */
#define F_WRLCK 1 /* 独占写锁 */
#define F_UNLCK 2 /* 解锁 */

/*
 * 文件锁结构定义
 * 用于描述文件锁的相关信息，目前未实现
 */
struct flock
{
    short l_type;   /* 锁的类型：F_RDLCK, F_WRLCK, F_UNLCK */
    short l_whence; /* 偏移量基准：SEEK_SET, SEEK_CUR, SEEK_END */
    off_t l_start;  /* 锁的起始位置（相对于 l_whence） */
    off_t l_len;    /* 锁的长度，0表示锁定到文件末尾 */
    pid_t l_pid;    /* 持有该锁的进程ID（仅用于 F_GETLK） */
};

/*
 * 函数声明
 */

/*
 * 创建一个新文件或截断现有文件
 * 参数：
 *   filename - 要创建的文件名
 *   mode - 文件的权限模式
 * 返回值：成功返回文件描述符，失败返回-1
 */
extern int creat(const char *filename, mode_t mode);

/*
 * 对文件描述符执行各种控制操作
 * 参数：
 *   fildes - 文件描述符
 *   cmd - 要执行的命令（F_DUPFD, F_GETFD等）
 *   ... - 可变参数，根据cmd的不同而变化
 * 返回值：根据cmd返回不同结果，失败返回-1
 */
extern int fcntl(int fildes, int cmd, ...);

/*
 * 打开一个文件
 * 参数：
 *   filename - 要打开的文件名
 *   flags - 打开标志（O_RDONLY, O_CREAT等组合）
 *   ... - 可变参数，当flags包含O_CREAT时，需要提供mode参数
 * 返回值：成功返回文件描述符，失败返回-1
 */
extern int open(const char *filename, int flags, ...);

#endif /* _FCNTL_H */