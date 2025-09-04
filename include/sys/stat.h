// 防止头文件被重复包含的宏定义
// 如果没有定义_SYS_STAT_H，则执行下面的代码
#ifndef _SYS_STAT_H
#define _SYS_STAT_H // 定义_SYS_STAT_H宏，标记头文件已包含

// 包含系统类型定义头文件，提供dev_t、ino_t等类型定义
#include <sys/types.h>

// 定义stat结构体，用于存储文件状态信息
struct stat
{
    dev_t st_dev;     // 文件所在设备的设备号
    ino_t st_ino;     // 文件的inode号，用于唯一标识文件
    umode_t st_mode;  // 文件类型和访问权限
    nlink_t st_nlink; // 文件的硬链接数
    uid_t st_uid;     // 文件所有者的用户ID
    gid_t st_gid;     // 文件所有者的组ID
    dev_t st_rdev;    // 特殊文件（如设备文件）的设备号
    off_t st_size;    // 普通文件的大小，以字节为单位
    time_t st_atime;  // 文件最后访问时间
    time_t st_mtime;  // 文件内容最后修改时间
    time_t st_ctime;  // 文件元数据（如权限）最后修改时间
};

// 文件类型掩码，用于从st_mode中提取文件类型信息
#define S_IFMT 00170000

// 各种文件类型的宏定义（八进制表示）
#define S_IFREG 0100000 // 普通文件
#define S_IFBLK 0060000 // 块设备文件
#define S_IFDIR 0040000 // 目录文件
#define S_IFCHR 0020000 // 字符设备文件
#define S_IFIFO 0010000 // FIFO管道文件

// 特殊权限位
#define S_ISUID 0004000 // 设置用户ID位（执行时切换为文件所有者权限）
#define S_ISGID 0002000 // 设置组ID位（执行时切换为文件所属组权限）
#define S_ISVTX 0001000 // 粘着位（仅对目录有效，防止非所有者删除文件）

// 文件类型检查宏，用于判断文件类型
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)  // 判断是否为普通文件
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)  // 判断是否为目录
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)  // 判断是否为字符设备
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)  // 判断是否为块设备
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO) // 判断是否为FIFO文件

// 文件所有者的权限位
#define S_IRWXU 00700 // 所有者具有读、写、执行权限（组合位）
#define S_IRUSR 00400 // 所有者的读权限
#define S_IWUSR 00200 // 所有者的写权限
#define S_IXUSR 00100 // 所有者的执行权限

// 文件所属组的权限位
#define S_IRWXG 00070 // 组具有读、写、执行权限（组合位）
#define S_IRGRP 00040 // 组的读权限
#define S_IWGRP 00020 // 组的写权限
#define S_IXGRP 00010 // 组的执行权限

// 其他用户的权限位
#define S_IRWXO 00007 // 其他用户具有读、写、执行权限（组合位）
#define S_IROTH 00004 // 其他用户的读权限
#define S_IWOTH 00002 // 其他用户的写权限
#define S_IXOTH 00001 // 其他用户的执行权限

// 声明与文件状态和权限相关的函数

// 更改文件的访问权限
extern int chmod(const char *_path, mode_t mode);

// 获取已打开文件的状态信息
extern int fstat(int fildes, struct stat *stat_buf);

// 创建目录
extern int mkdir(const char *_path, mode_t mode);

// 创建FIFO管道文件
extern int mkfifo(const char *_path, mode_t mode);

// 获取指定文件的状态信息
extern int stat(const char *filename, struct stat *stat_buf);

// 设置文件创建时的权限掩码（umask）
extern mode_t umask(mode_t mask);

#endif // 结束头文件条件编译块