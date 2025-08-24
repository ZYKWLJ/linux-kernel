/*
 * 本文件包含软盘控制器的一些定义
 * 来源多样，主要参考《IBM微型计算机：程序员手册》（Sanches和Canton著）
 */
#ifndef _FDREG_H  // 防止头文件重复包含的宏定义
#define _FDREG_H

// 外部函数声明
// 计算使第nr个软盘驱动器启动所需的滴答数
extern int ticks_to_floppy_on(unsigned int nr);
// 开启第nr个软盘驱动器
extern void floppy_on(unsigned int nr);
// 关闭第nr个软盘驱动器
extern void floppy_off(unsigned int nr);
// 选择第nr个软盘驱动器
extern void floppy_select(unsigned int nr);
// 取消选择第nr个软盘驱动器
extern void floppy_deselect(unsigned int nr);

/* 软盘控制器寄存器地址（参考Sanches和Canton的书，约340页） */
#define FD_STATUS	0x3f4	// 状态寄存器
#define FD_DATA		0x3f5	// 数据寄存器
#define FD_DOR		0x3f2	// 数字输出寄存器(Digital Output Register)
#define FD_DIR		0x3f7	// 数字输入寄存器(读操作)(Digital Input Register)
#define FD_DCR		0x3f7	// 软盘控制寄存器(写操作)(Diskette Control Register)

/* 主状态寄存器的位定义 */
#define STATUS_BUSYMASK	0x0F	// 驱动器忙状态掩码（用于提取驱动器忙状态）
#define STATUS_BUSY	0x10	// FDC(软盘控制器)忙
#define STATUS_DMA	0x20	// DMA模式标志（0表示处于DMA模式）
#define STATUS_DIR	0x40	// 数据传输方向（0表示CPU到FDC）
#define STATUS_READY	0x80	// 数据寄存器就绪（可以进行读写操作）

/* FD_ST0状态寄存器的位定义 */
#define ST0_DS		0x03	// 驱动器选择掩码（用于提取被选中的驱动器）
#define ST0_HA		0x04	// 磁头地址(Head Address)
#define ST0_NR		0x08	// 未就绪(Not Ready)
#define ST0_ECE		0x10	// 设备检查错误(Equipment Check Error)
#define ST0_SE		0x20	// 寻道结束(Seek End)
#define ST0_INTR	0xC0	// 中断代码掩码（用于提取中断代码）

/* FD_ST1状态寄存器的位定义 */
#define ST1_MAM		0x01	// 丢失地址标记(Missing Address Mark)
#define ST1_WP		0x02	// 写保护(Write Protect)
#define ST1_ND		0x04	// 无数据（不可读）(No Data - unreadable)
#define ST1_OR		0x10	// 溢出(OverRun)
#define ST1_CRC		0x20	// 数据或地址中的CRC错误
#define ST1_EOC		0x80	// 柱面结束(End Of Cylinder)

/* FD_ST2状态寄存器的位定义 */
#define ST2_MAM		0x01	// 丢失地址标记（再次出现）(Missing Address Mark)
#define ST2_BC		0x02	// 坏柱面(Bad Cylinder)
#define ST2_SNS		0x04	// 扫描未完成(Scan Not Satisfied)
#define ST2_SEH		0x08	// 扫描匹配命中(Scan Equal Hit)
#define ST2_WC		0x10	// 错误柱面(Wrong Cylinder)
#define ST2_CRC		0x20	// 数据字段中的CRC错误
#define ST2_CM		0x40	// 控制标记=已删除(Control Mark = deleted)

/* FD_ST3状态寄存器的位定义 */
#define ST3_HA		0x04	// 磁头地址(Head Address)
#define ST3_TZ		0x10	// 零磁道信号（1表示在0磁道）(Track Zero signal)
#define ST3_WP		0x40	// 写保护(Write Protect)

/* 软盘控制器命令值 */
#define FD_RECALIBRATE	0x07	// 重新校准（移动到0磁道）
#define FD_SEEK		0x0F	// 寻道磁道
#define FD_READ		0xE6	// 读操作（带MT, MFM, 跳过已删除）
#define FD_WRITE	0xC5	// 写操作（带MT, MFM）
#define FD_SENSEI	0x08	// 检测中断状态(Sense Interrupt Status)
#define FD_SPECIFY	0x03	// 指定HUT等参数

/* DMA命令 */
#define DMA_READ	0x46	// DMA读命令
#define DMA_WRITE	0x4A	// DMA写命令

#endif  // _FDREG_H宏定义结束