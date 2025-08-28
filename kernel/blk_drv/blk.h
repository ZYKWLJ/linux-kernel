// 防止头文件重复包含的宏定义
#ifndef _BLK_H
#define _BLK_H

// 定义系统支持的块设备数量为7个
#define NR_BLK_DEV 7

/*
 * NR_REQUEST 是请求队列中的条目数量
 * 注意：写操作只能使用其中的2/3，读操作有更高的优先级
 * 
 * 32是一个合理的数值：既能从电梯调度算法中获益，
 * 又不会在队列中锁定过多缓冲区。64则太多了（在大量写操作/同步时，
 * 很容易导致读操作长时间停顿）
 */
#define NR_REQUEST 32

/*
 * 这是一个扩展形式，以便在实现分页请求时可以使用相同的请求结构。
 * 在分页中，'bh'为NULL，'waiting'用于等待读/写完成。
 */
// 定义块设备I/O请求结构体
struct request
{
    int dev;                 /* 设备号，-1表示无请求 */
    int cmd;                 /* 命令，READ或WRITE */
    int errors;              /* 错误计数 */
    unsigned long sector;    /* 起始扇区号 */
    unsigned long nr_sectors;/* 要传输的扇区数 */
    char *buffer;            /* 数据缓冲区指针 */
    struct task_struct *waiting; /* 等待该请求完成的任务 */
    struct buffer_head *bh;  /* 缓冲区头指针 */
    struct request *next;    /* 指向下一个请求，用于形成链表 */
};

/*
 * 这用于电梯调度算法：注意读操作总是在写操作之前。
 * 这很自然：读操作比写操作对时间更敏感。
 */
// 定义IN_ORDER宏，用于电梯调度算法中判断两个请求的顺序
#define IN_ORDER(s1, s2)                                                            \
    ((s1)->cmd < (s2)->cmd || ((s1)->cmd == (s2)->cmd &&                            \
                               ((s1)->dev < (s2)->dev || ((s1)->dev == (s2)->dev && \
                                                          (s1)->sector < (s2)->sector))))

// 定义块设备结构体
struct blk_dev_struct
{
    void (*request_fn)(void);    /* 处理请求的函数指针 */
    struct request *current_request; /* 当前正在处理的请求 */
};

// 声明块设备数组、请求数组和等待请求的任务
extern struct blk_dev_struct blk_dev[NR_BLK_DEV];
extern struct request request[NR_REQUEST];
extern struct task_struct *wait_for_request;

// 如果定义了MAJOR_NR（主设备号），则进行以下设备特定定义
#ifdef MAJOR_NR

/*
 * 根据需要添加条目。目前仅支持硬盘和软盘等块设备。
 */

// 如果是主设备号1（内存盘）
#if (MAJOR_NR == 1)
/* ram disk */
#define DEVICE_NAME "ramdisk"          /* 设备名称 */
#define DEVICE_REQUEST do_rd_request   /* 设备请求处理函数 */
#define DEVICE_NR(device) ((device) & 7) /* 设备编号计算 */
#define DEVICE_ON(device)              /* 设备开启操作（空实现） */
#define DEVICE_OFF(device)             /* 设备关闭操作（空实现） */

// 如果是主设备号2（软盘）
#elif (MAJOR_NR == 2)
/* floppy */
#define DEVICE_NAME "floppy"           /* 设备名称 */
#define DEVICE_INTR do_floppy          /* 设备中断处理函数 */
#define DEVICE_REQUEST do_fd_request   /* 设备请求处理函数 */
#define DEVICE_NR(device) ((device) & 3) /* 设备编号计算 */
#define DEVICE_ON(device) floppy_on(DEVICE_NR(device)) /* 设备开启操作 */
#define DEVICE_OFF(device) floppy_off(DEVICE_NR(device)) /* 设备关闭操作 */

// 如果是主设备号3（硬盘）
#elif (MAJOR_NR == 3)
/* harddisk */
#define DEVICE_NAME "harddisk"         /* 设备名称 */
#define DEVICE_INTR do_hd              /* 设备中断处理函数 */
#define DEVICE_REQUEST do_hd_request   /* 设备请求处理函数 */
#define DEVICE_NR(device) (MINOR(device) / 5) /* 设备编号计算 */
#define DEVICE_ON(device)              /* 设备开启操作（空实现） */
#define DEVICE_OFF(device)             /* 设备关闭操作（空实现） */

// 未知块设备
#else
/* unknown blk device */
#error "unknown blk device"  /* 编译错误：未知块设备 */

#endif

// 定义当前请求宏，指向当前设备的当前请求
#define CURRENT (blk_dev[MAJOR_NR].current_request)
// 定义当前设备宏，获取当前请求的设备编号
#define CURRENT_DEV DEVICE_NR(CURRENT->dev)

// 如果定义了设备中断处理函数，则声明该函数指针
#ifdef DEVICE_INTR
void (*DEVICE_INTR)(void) = NULL;
#endif

// 声明设备请求处理函数
static void(DEVICE_REQUEST)(void);

// 解锁缓冲区的内联函数
static inline void unlock_buffer(struct buffer_head *bh)
{
    // 如果缓冲区未被锁定，打印警告信息
    if (!bh->b_lock)
        printk(DEVICE_NAME ": free buffer being unlocked\n");
    // 清除锁定标志
    bh->b_lock = 0;
    // 唤醒等待该缓冲区的进程
    wake_up(&bh->b_wait);
}

// 结束请求处理的内联函数
static inline void end_request(int uptodate)
{
    // 关闭当前设备
    DEVICE_OFF(CURRENT->dev);
    // 如果存在缓冲区头
    if (CURRENT->bh)
    {
        // 设置缓冲区更新状态
        CURRENT->bh->b_uptodate = uptodate;
        // 解锁缓冲区
        unlock_buffer(CURRENT->bh);
    }
    // 如果操作失败
    if (!uptodate)
    {
        // 打印I/O错误信息
        printk(DEVICE_NAME " I/O error\n\r");
        printk("dev %04x, block %d\n\r", CURRENT->dev,
               CURRENT->bh->b_blocknr);
    }
    // 唤醒等待该请求完成的进程
    wake_up(&CURRENT->waiting);
    // 唤醒等待请求队列有空位的进程
    wake_up(&wait_for_request);
    // 标记当前请求为无效
    CURRENT->dev = -1;
    // 处理下一个请求
    CURRENT = CURRENT->next;
}

// 初始化请求处理的宏定义
#define INIT_REQUEST                                   \
    repeat:                                            \
    if (!CURRENT)                                      \
        return;                                        \
    if (MAJOR(CURRENT->dev) != MAJOR_NR)               \
        panic(DEVICE_NAME ": request list destroyed"); \
    if (CURRENT->bh)                                   \
    {                                                  \
        if (!CURRENT->bh->b_lock)                      \
            panic(DEVICE_NAME ": block not locked");   \
    }

#endif  // 结束MAJOR_NR条件编译

#endif  // 结束头文件保护宏