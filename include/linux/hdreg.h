/*
 * 此文件包含 AT 硬盘控制器（AT-hd-controller）的相关宏定义。
 * 定义来源多样，部分定义的细节可参考注释（带有问号的注释表示待确认/需结合硬件手册的细节）。
 */
#ifndef _HDREG_H  // 防止头文件重复包含的保护宏：若未定义 _HDREG_H
#define _HDREG_H  // 则定义 _HDREG_H，确保文件内容仅编译一次

/* 硬盘控制器端口地址定义，参考 IBM AT 主板 BIOS 列表 */
// 硬盘数据端口：读操作时为数据输入端口，写操作时配合控制信号使用（_CTL 表示控制相关）
#define HD_DATA 0x1f0        /* _CTL when writing */
// 硬盘错误端口：读取时获取错误状态（对应 err-bits 定义的错误位）
#define HD_ERROR 0x1f1       /* see err-bits */
// 扇区数端口：指定要读取/写入的扇区总数
#define HD_NSECTOR 0x1f2     /* nr of sectors to read/write */
// 起始扇区端口：指定读写操作的起始扇区号
#define HD_SECTOR 0x1f3      /* starting sector */
// 柱面低字节端口：起始柱面号的低 8 位
#define HD_LCYL 0x1f4        /* starting cylinder (low byte) */
// 柱面高字节端口：起始柱面号的高 8 位（与 HD_LCYL 共同组成 16 位柱面号）
#define HD_HCYL 0x1f5        /* high byte of starting cyl */
// 当前磁头/驱动器端口：bit7-bit4 为磁头号（hhhh），bit3 为驱动器号（d，0=主盘，1=从盘），bit2-bit0 固定为 101
#define HD_CURRENT 0x1f6     /* 101dhhhh , d=drive, hhhh=head */
// 状态端口：读取时获取硬盘控制器的当前状态（对应 status-bits 定义的状态位）
#define HD_STATUS 0x1f7      /* see status-bits */
// 写预补偿端口：与 HD_ERROR 共用同一 IO 地址，读操作时是错误端口，写操作时用于设置写预补偿参数
#define HD_PRECOMP HD_ERROR  /* same io address, read=error, write=precomp */
// 命令端口：与 HD_STATUS 共用同一 IO 地址，读操作时是状态端口，写操作时用于向控制器发送命令
#define HD_COMMAND HD_STATUS /* same io address, read=status, write=cmd */

// 硬盘控制端口（辅助控制，如复位、中断允许等）
#define HD_CMD 0x3f6

/* HD_STATUS（状态端口）的位定义（每一位代表一种状态） */
#define ERR_STAT 0x01    // 位0：错误状态（控制器检测到错误）
#define INDEX_STAT 0x02  // 位1：索引状态（检测到磁盘索引信号，标记磁道起始）
#define ECC_STAT 0x04    // 位2：ECC 校正状态（控制器通过 ECC 算法修正了数据错误）/* Corrected error */
#define DRQ_STAT 0x08    // 位3：数据请求状态（控制器已准备好接收/发送数据，等待 CPU 传输）
#define SEEK_STAT 0x10   // 位4：寻道完成状态（磁头已移动到指定柱面）
#define WRERR_STAT 0x20  // 位5：写错误状态（写操作失败）
#define READY_STAT 0x40  // 位6：就绪状态（硬盘已上电并准备好接收命令）
#define BUSY_STAT 0x80   // 位7：忙状态（控制器正在执行命令，无法接收新命令）

/* HD_COMMAND（命令端口）的命令值定义（向控制器发送的操作指令） */
#define WIN_RESTORE 0x10    // 磁头归位命令（将磁头移动到第 0 磁道）
#define WIN_READ 0x20       // 读扇区命令（从指定扇区读取数据）
#define WIN_WRITE 0x30      // 写扇区命令（向指定扇区写入数据）
#define WIN_VERIFY 0x40     // 扇区校验命令（检查指定扇区数据是否可正常读取，不传输数据）
#define WIN_FORMAT 0x50     // 格式化磁道命令（格式化指定磁道，清除数据）
#define WIN_INIT 0x60       // 初始化驱动器命令（初始化硬盘控制器参数）
#define WIN_SEEK 0x70       // 寻道命令（将磁头移动到指定柱面，不读写数据）
#define WIN_DIAGNOSE 0x90   // 诊断命令（执行控制器自检，返回诊断结果）
#define WIN_SPECIFY 0x91    // 设置驱动器参数命令（向控制器写入硬盘参数，如磁头数、扇区数）

/* HD_ERROR（错误端口）的错误位定义（每一位代表一种错误类型） */
#define MARK_ERR 0x01   // 位0：地址标记错误（未找到扇区地址标记）/* Bad address mark ? */
#define TRK0_ERR 0x02   // 位1：0 磁道错误（磁头无法移动到第 0 磁道）/* couldn't find track 0 */
#define ABRT_ERR 0x04   // 位2：命令中止错误（控制器中止了当前命令，原因待查）/* ? */
#define ID_ERR 0x10     // 位4：ID 字段错误（扇区 ID 信息读取失败或不匹配）/* ? */
#define ECC_ERR 0x40    // 位6：ECC 错误（ECC 算法无法修正数据错误）/* ? */
#define BBD_ERR 0x80    // 位7：坏块检测错误（检测到坏扇区）/* ? */

// 分区表结构体：存储硬盘分区的关键信息（每个分区表项占 16 字节，硬盘主引导扇区包含 4 个该结构体）
struct partition
{
    unsigned char boot_ind;   /* 引导标志：0x80 表示该分区为可引导分区，0x00 表示不可引导（当前未实际使用） */
    unsigned char head;       /* 分区起始磁头号（对应硬盘物理磁头）/* ? */ */
    unsigned char sector;     /* 分区起始扇区号：低 6 位为扇区号，高 2 位为起始柱面号的高 2 位/* ? */ */
    unsigned char cyl;        /* 分区起始柱面号的低 8 位（与 sector 的高 2 位组成完整柱面号）/* ? */ */
    unsigned char sys_ind;    /* 分区类型标志：标识分区的文件系统类型（如 0x83 为 Linux 分区）/* ? */ */
    unsigned char end_head;   /* 分区结束磁头号（分区最后一个扇区所在的磁头）/* ? */ */
    unsigned char end_sector; /* 分区结束扇区号：低 6 位为扇区号，高 2 位为结束柱面号的高 2 位/* ? */ */
    unsigned char end_cyl;    /* 分区结束柱面号的低 8 位（与 end_sector 的高 2 位组成完整柱面号）/* ? */ */
    unsigned int start_sect;  /* 分区起始扇区号：从硬盘第 0 扇区开始计数的绝对扇区号 */
    unsigned int nr_sects;    /* 分区总扇区数：该分区包含的扇区总数（决定分区容量） */
};

#endif  // 结束头文件保护宏，对应开头的 #ifndef _HDREG_H