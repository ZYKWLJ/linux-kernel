#ifndef _FS_H
#define _FS_H

#include <sys/types.h>

// 注释说明设备的主设备号定义（与 Minix 系统兼容，以便使用 Minix 文件系统），
// 列出了各主设备号对应的设备：

// 0：未使用
// 1：内存设备 (/dev/mem)
// 2：文件描述符设备 (/dev/fd)
// 3：硬盘设备 (/dev/hd)
// 4：终端设备 (/dev/ttyx)
// 5：终端设备 (/dev/tty)
// 6：打印机设备 (/dev/lp)
// 7：无名管道

// 判断设备是否可 seek（可定位），主设备号 1-3 的设备是可定位的

#define IS_SEEKABLE(x) ((x) >= 1 && (x) <= 3)

// 定义 I/O 操作类型：读、写、预读、预写

#define READ 0
#define WRITE 1
#define READA 2  /* read-ahead - dont pause */
#define WRITEA 3 /* "write-ahead" - silly, but somewhat useful */

// 声明缓冲区初始化函数
void buffer_init(long buffer_end);

// 从设备号中提取主设备号 (MAJOR) 和次设备号 (MINOR)
// 主设备号在高 8 位，次设备号在低 8 位
#define MAJOR(a) (((unsigned)(a)) >> 8)
#define MINOR(a) ((a) & 0xff)

// 文件名最大长度为 14 个字符
#define NAME_LEN 14
// 根目录的 i 节点号为 1
#define ROOT_INO 1

// i 节点位图和 zone 位图的槽位数都是 8

#define I_MAP_SLOTS 8
#define Z_MAP_SLOTS 8

// 文件系统超级块的魔数（用于识别文件系统类型）
#define SUPER_MAGIC 0x137F

// 系统限制常量：
// 每个进程最大打开文件数：20

#define NR_OPEN 20
// inode 表大小：32
#define NR_INODE 32
// 文件表大小：64
#define NR_FILE 64
// 超级块数量：8
#define NR_SUPER 8
// 哈希表大小：307
#define NR_HASH 307
// 缓冲区数量：nr_buffers（外部定义）
#define NR_BUFFERS nr_buffers
// 块大小：1024 字节
#define BLOCK_SIZE 1024
// 块大小的比特数：10（2^10=1024）
#define BLOCK_SIZE_BITS 10
// 定义 NULL 指针
#ifndef NULL
#define NULL ((void *)0)
#endif

// 每个块可容纳的 inode 数量
#define INODES_PER_BLOCK ((BLOCK_SIZE) / (sizeof(struct d_inode)))

// 每个块可容纳的目录项数量
#define DIR_ENTRIES_PER_BLOCK ((BLOCK_SIZE) / (sizeof(struct dir_entry)))
/**
* func descp: 管道操作相关宏定义
*/

// 管道头指针
#define PIPE_HEAD(inode) ((inode).i_zone[0])
// 管道尾指针
#define PIPE_TAIL(inode) ((inode).i_zone[1])
// 管道大小计算
#define PIPE_SIZE(inode) ((PIPE_HEAD(inode) - PIPE_TAIL(inode)) & (PAGE_SIZE - 1))
// 判断管道空
#define PIPE_EMPTY(inode) (PIPE_HEAD(inode) == PIPE_TAIL(inode))
// 判断管道满
#define PIPE_FULL(inode) (PIPE_SIZE(inode) == (PAGE_SIZE - 1))

// 原子操作递增管道头指针（使用内联汇编）
#define INC_PIPE(head) \
    __asm__("incl %0\n\tandl $4095,%0" ::"m"(head))
// 定义缓冲区块类型，大小为 BLOCK_SIZE (1024 字节)
typedef char buffer_block[BLOCK_SIZE];

// 缓冲区头结构，用于管理磁盘缓冲区
struct buffer_head
{
    char *b_data;            /* pointer to data block (1024 bytes) */
    unsigned long b_blocknr; /* block number */
    unsigned short b_dev;    /* device (0 = free) */
    unsigned char b_uptodate; /* 数据是否最新 */
    unsigned char b_dirt;  /* 0-clean,1-dirty 数据是否修改过 */
    unsigned char b_count; /* 使用该块的用户数 */
    unsigned char b_lock;  /* 0 - ok, 1 -locked 是否锁定 */
    struct task_struct *b_wait; /* 等待该缓冲区的任务 */
    struct buffer_head *b_prev; /* 哈希表前向指针 */
    struct buffer_head *b_next; /* 哈希表后向指针 */
    struct buffer_head *b_prev_free; /* 空闲链表前向指针 */
    struct buffer_head *b_next_free; /* 空闲链表后向指针 */
};
  
// 磁盘上存储的 inode 结构，包含文件的元数据
struct d_inode
{
    unsigned short i_mode;  /* 文件类型和权限 */
    unsigned short i_uid;   /* 用户ID */
    unsigned long i_size;   /* 文件大小 */
    unsigned long i_time;   /* 修改时间 */
    unsigned char i_gid;    /* 组ID */
    unsigned char i_nlinks; /* 链接数 */
    unsigned short i_zone[9]; /* 数据块索引 */
};

// 内存中的 inode 结构，比磁盘的 inode 多了一些运行时需要的字段
struct m_inode
{
    unsigned short i_mode;
    unsigned short i_uid;
    unsigned long i_size;
    unsigned long i_mtime;  /* 修改时间 */
    unsigned char i_gid;
    unsigned char i_nlinks;
    unsigned short i_zone[9];

    /* 以下是仅在内存中的字段 */
    struct task_struct *i_wait; /* 等待该inode的任务 */
    unsigned long i_atime;  /* 访问时间 */
    unsigned long i_ctime;  /* 创建时间 */
    unsigned short i_dev;   /* 设备号 */
    unsigned short i_num;   /* inode号 */
    unsigned short i_count; /* 引用计数 */
    unsigned char i_lock;   /* 锁定标志 */
    unsigned char i_dirt;   /* 脏标志（是否被修改） */
    unsigned char i_pipe;   /* 是否为管道 */
    unsigned char i_mount;  /* 是否为挂载点 */
    unsigned char i_seek;   /* 寻道标志 */
    unsigned char i_update; /* 更新标志 */
};

// 文件结构，代表一个打开的文件
struct file
{
    unsigned short f_mode;  /* 文件模式（读写等） */
    unsigned short f_flags; /* 文件标志 */
    unsigned short f_count; /* 引用计数 */
    struct m_inode *f_inode; /* 关联的inode */
    off_t f_pos;            /* 文件当前位置 */
};

// 超级块结构，包含整个文件系统的元数据
struct super_block
{
    unsigned short s_ninodes; /* inode总数 */
    unsigned short s_nzones;  /* 数据块总数 */
    unsigned short s_imap_blocks; /* inode位图占用的块数 */
    unsigned short s_zmap_blocks; /* 数据块位图占用的块数 */
    unsigned short s_firstdatazone; /* 第一个数据块的位置 */
    unsigned short s_log_zone_size; /* 块大小的对数 */
    unsigned long s_max_size;      /* 最大文件大小 */
    unsigned short s_magic;        /* 魔数 */
    /* 以下是仅在内存中的字段 */
    struct buffer_head *s_imap[8]; /* inode位图缓冲区 */
    struct buffer_head *s_zmap[8]; /* 数据块位图缓冲区 */
    unsigned short s_dev;          /* 设备号 */
    struct m_inode *s_isup;        /* 超级用户inode */
    struct m_inode *s_imount;      /* 挂载的inode */
    unsigned long s_time;          /* 超级块修改时间 */
    struct task_struct *s_wait;    /* 等待超级块的任务 */
    unsigned char s_lock;          /* 锁定标志 */
    unsigned char s_rd_only;       /* 只读标志 */
    unsigned char s_dirt;          /* 脏标志 */
};

// 磁盘上存储的超级块结构，比内存中的超级块简单

struct d_super_block
{
    unsigned short s_ninodes;
    unsigned short s_nzones;
    unsigned short s_imap_blocks;
    unsigned short s_zmap_blocks;
    unsigned short s_firstdatazone;
    unsigned short s_log_zone_size;
    unsigned long s_max_size;
    unsigned short s_magic;
};

// 目录项结构，每个目录项包含文件名和对应的 inode 号

struct dir_entry
{
    unsigned short inode;   /* 对应的inode号 */
    char name[NAME_LEN];    /* 文件名 */
};

/**
* data descp: 声明了文件系统的全局数据结构，即inode表、文件表、超级块数组、起始位置、缓冲区块总数。
*/
extern struct m_inode inode_table[NR_INODE];/*inode 表*/
extern struct file file_table[NR_FILE];/*文件表*/
extern struct super_block super_block[NR_SUPER];/*超级块数组*/
extern struct buffer_head *start_buffer;/*指向缓冲区链表的起始位置，是整个缓冲区管理的入口点。*/
extern int nr_buffers;/*表示系统中实际可用的缓冲区块总数。*/
/**
* func descp: 声明了文件系统操作的各种函数，包括设备管理、inode 操作、块操作等
*/
  
/**
* data descp: 设备相关函数
*/

// 检查指定设备（dev为设备号）是否发生介质更换（如软盘被换盘）。
// 若检测到更换，会刷新相关缓存（如 inode、缓冲区），避免访问旧介质的数据。
extern void check_disk_change(int dev);
// 检查编号为nr的软盘驱动器是否发生换盘。返回值通常为 0（未更换）或非 0（已更换），
// 用于软盘介质变更的检测。
extern int floppy_change(unsigned int nr);
// 计算软盘驱动器（dev指定设备）从当前状态到启动就绪所需的时钟滴答数（系统时间单位）。
// 用于预估软盘启动延迟，优化 I/O 调度。
extern int ticks_to_floppy_on(unsigned int dev);
// 启动指定的软盘驱动器（dev），为后续读写操作做准备（如激活电机）。
extern void floppy_on(unsigned int dev);
// 关闭指定的软盘驱动器（dev），停止电机以节省功耗。通常在一段时间无操作后调用。
extern void floppy_off(unsigned int dev);
/**
* data descp: inode 与文件操作函数
*/
// 截断文件：释放 inode（inode）对应文件的所有数据块，
// 将文件大小设为 0。常用于删除文件内容或清空文件。
extern void truncate(struct m_inode *inode);
// 将内存中所有被修改过的 inode（i_dirt标记为 1）同步到磁盘，确保元数据持久化，防止数据丢失。
extern void sync_inodes(void);
// 等待 inode（inode）上的锁定（i_lock）释放。
// 若 inode 被其他进程锁定，当前进程会进入睡眠状态，直到锁定解除，用于实现 inode 的互斥访问。
extern void wait_on(struct m_inode *inode);
// 查找文件逻辑块（block）对应的物理磁盘块号。
// 通过 inode 的i_zone数组（间接块索引）映射逻辑块到物理块，返回物理块号（失败返回 - 1）。
extern int bmap(struct m_inode *inode, int block);
// 为文件的逻辑块（block）分配物理磁盘块，并更新 inode 的i_zone数组记录映射关系。
// 返回新分配的物理块号（失败返回 - 1）。
extern int create_block(struct m_inode *inode, int block);
/**
* data descp: 路径解析与 inode 管理
*/
// 解析文件路径（pathname），返回对应的 inode 指针。
// 用于根据路径查找文件的元数据，支持绝对路径和相对路径。
extern struct m_inode *namei(const char *pathname);
// 解析路径（pathname）并根据操作标志（flag，如读写模式）和权限（mode）打开文件，
// 将结果 inode 存入res_inode。返回操作状态（0 表示成功），支持创建新文件（若路径不存在且有创建权限）。
extern int open_namei(const char *pathname, int flag, int mode,
                      struct m_inode **res_inode);
// 减少 inode 的引用计数（i_count）。当计数减为 0 时，
// 若 inode 被修改（i_dirt=1），则同步到磁盘，并将其标记为空闲，供其他文件复用。
extern void iput(struct m_inode *inode);
// 从指定设备（dev）加载编号为nr的 inode 到内存 inode 表（inode_table）。
// 若 inode 已在内存中，则直接返回并增加引用计数；否则从磁盘读取并初始化。
extern struct m_inode *iget(int dev, int nr);
// 从 inode 表（inode_table）中获取一个空闲的 inode 结构，
// 初始化其字段（如引用计数、锁定状态等），用于创建新文件时分配 inode。
extern struct m_inode *get_empty_inode(void);
// 创建一个用于管道（pipe）的专用 inode，
// 初始化管道相关字段（如i_pipe标记、管道缓冲区指针），支持进程间通信。
extern struct m_inode *get_pipe_inode(void);
/**
* data descp: 缓冲区管理函数
*/

// 在缓冲区哈希表中查找指定设备（dev）和块号（block）对应的缓冲区头（buffer_head）。
// 哈希表通过设备号和块号快速定位缓存的磁盘块，减少磁盘 I/O。
extern struct buffer_head *get_hash_table(int dev, int block);
// 为指定设备（dev）的块号（block）分配一个缓冲区。若该块已在缓存中，则返回现有缓冲区；
// 否则从空闲链表中分配一个新缓冲区，并加入哈希表。
extern struct buffer_head *getblk(int dev, int block);
// 底层读写磁盘块：rw指定操作类型（READ或WRITE），bh为缓冲区头。直接与磁盘驱动交互，
// 将缓冲区数据写入磁盘或从磁盘读入缓冲区。
extern void ll_rw_block(int rw, struct buffer_head *bh);
// 释放缓冲区（buf）的引用计数（b_count）。当计数为 0 时，将缓冲区加入空闲链表，供其他操作复用。
extern void brelse(struct buffer_head *buf);
// 读取指定设备（dev）的块号（block）到缓冲区。若块已在缓存中，直接返回缓冲区；
// 否则调用ll_rw_block从磁盘读取，并等待操作完成。
extern struct buffer_head *bread(int dev, int block);
// 读取 4 个连续的磁盘块（b[0]到b[3]）到内存地址addr开始的页面中。
// 用于高效加载文件的一个内存页（通常 4KB，对应 4 个 1KB 磁盘块）。
extern void bread_page(unsigned long addr, int dev, int b[4]);
// 预读功能：读取指定块（block）及后续多个块（通过可变参数指定）到缓冲区，
// 提前缓存可能用到的数据，减少后续 I/O 延迟（类似预加载）。
extern struct buffer_head *breada(int dev, int block, ...);
/**
* data descp:  块与 inode 分配释放
*/
// 在指定设备（dev）上分配一个空闲的磁盘块。通过超级块的块位图（s_zmap）查找空闲块，
// 标记为已使用，并返回块号（失败返回 0）。
extern int new_block(int dev);
// 释放指定设备（dev）上的块号（block），将其标记为空闲（更新块位图），供后续分配复用。
extern void free_block(int dev, int block);
// 在指定设备（dev）上分配一个新的 inode。通过超级块的 inode 位图（s_imap）查找空闲 inode，
// 初始化其字段，并返回 inode 指针（失败返回 NULL）。
extern struct m_inode *new_inode(int dev);
// 释放 inode（inode），将其在 inode 位图中标记为空闲，同时清除 inode 的元数据，供新文件使用。
extern void free_inode(struct m_inode *inode);
// 同步指定设备（dev）的所有缓冲区数据和 inode 到磁盘，
// 确保该设备上的所有修改持久化（类似fsync系统调用的底层实现）。
extern int sync_dev(int dev);
// 获取指定设备（dev）对应的超级块（super_block）。
// 若超级块已加载到内存（super_block数组），则直接返回；否则从磁盘读取并初始化。
extern struct super_block *get_super(int dev);
// 根文件系统的设备号（全局变量），指定系统启动时挂载的根分区（如软盘或硬盘分区）。
extern int ROOT_DEV;
// 挂载根文件系统：初始化根设备（ROOT_DEV）的超级块，加载根目录的 inode（ROOT_INO=1），
// 作为整个文件系统的起点。这是系统启动过程中的关键步骤。
extern void mount_root(void);

#endif