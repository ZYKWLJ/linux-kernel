/*
 *  linux/fs/file_table.c
 *  内核文件系统模块：文件表（file table）管理实现文件
 *  功能：维护系统级"打开文件列表"，记录所有进程打开文件的核心状态（如读写位置、访问模式）
 *
 *  (C) 1991  Linus Torvalds  // 作者及年份，属于Linux内核早期核心代码
 */

// 包含文件系统核心头文件 <linux/fs.h>
// 该头文件定义了：
// 1. struct file（文件表项结构）、struct inode（索引节点结构）等核心数据结构
// 2. 文件操作相关宏（如O_RDONLY、O_WRONLY）、函数声明（如open、read、write）
// 3. 文件系统常量（如NR_FILE：系统最大打开文件数）
#include <linux/fs.h>

/* 
 * 定义系统级文件表：存储所有当前打开的文件
 * - struct file：每个表项对应一个"打开的文件实例"，记录该文件的动态状态（如当前读写偏移、打开模式）
 * - file_table：数组形式的文件表，是系统全局资源，所有进程共享
 * - NR_FILE：编译时定义的常量（Linux 0.11中默认值为32），限制系统同时打开的最大文件数量
 * 
 * 核心作用：
 * 进程通过"文件描述符"（fd，如0=标准输入、1=标准输出）索引自己的"文件描述符表"，
 * 而文件描述符表的表项指向 file_table 中的某个 struct file，
 * 多个进程可通过不同fd指向同一个 struct file（实现文件共享，如父子进程共享打开的文件）
 */
struct file file_table[NR_FILE];