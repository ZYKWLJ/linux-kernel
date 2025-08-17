/**
* func descp: 代码文件总体分析
*/
// 定义了一系列用于在 Linux 内核中访问用户空间内存的内联函数，
// 主要通过内嵌汇编操作段寄存器fs来实现内核与用户空间的数据交互。以下是逐行解析：

// 功能：从用户空间地址addr读取 1 个字节（8 位）。
// 汇编解析：movb %%fs:%1,%0表示从fs段选择符指定的内存段中，
// 读取%1（即*addr指向的内存）的 1 字节数据到%0（即变量_v）。
// "=r" (_v)表示_v是输出寄存器，"m" (*addr)表示*addr是内存输入。

static inline unsigned char get_fs_byte(const char * addr)
{
	unsigned register char _v;

	__asm__ ("movb %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

// 功能：从用户空间地址addr读取 1 个字（16 位）。
// 汇编解析：movw是 16 位数据传输指令，从fs段的*addr读取 2 字节到_v。
static inline unsigned short get_fs_word(const unsigned short *addr)
{
	unsigned short _v;

	__asm__ ("movw %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

// 功能：从用户空间地址addr读取 1 个长字（32 位）。
// 汇编解析：movl是 32 位数据传输指令，从fs段的*addr读取 4 字节到_v。
static inline unsigned long get_fs_long(const unsigned long *addr)
{
	unsigned long _v;

	__asm__ ("movl %%fs:%1,%0":"=r" (_v):"m" (*addr)); \
	return _v;
}

// 功能：向用户空间地址addr写入 1 个字节（8 位）。
// 汇编解析：movb %0,%%fs:%1表示将%0（即val）的 1 字节数据写入fs段的%1
// （即*addr指向的内存）。"r" (val)表示val是寄存器输入，"m" (*addr)表示*addr是内存输出。

static inline void put_fs_byte(char val,char *addr)
{
__asm__ ("movb %0,%%fs:%1"::"r" (val),"m" (*addr));
}

// 功能：向用户空间地址addr写入 1 个字（16 位）。
// 汇编解析：movw是 16 位传输指令，将val写入fs段的*addr。
static inline void put_fs_word(short val,short * addr)
{
__asm__ ("movw %0,%%fs:%1"::"r" (val),"m" (*addr));
}

// 功能：向用户空间地址addr写入 1 个长字（32 位）。
// 汇编解析：movl是 32 位传输指令，将val写入fs段的*addr。
static inline void put_fs_long(unsigned long val,unsigned long * addr)
{
__asm__ ("movl %0,%%fs:%1"::"r" (val),"m" (*addr));
}

/*
 *要是有谁比我更懂 GNU 汇编，还请帮忙再检查下下面这段代码。
 *它目前看起来能正常工作，但我不确定自己有没有犯什么不易察觉的错误。
 *——TYT，1991 年 11 月 24 日
 *[这段代码没问题，林纳斯] // 林纳斯（Linux 创始人）确认代码无误
 */

 // 功能：获取当前fs段寄存器的值（16 位段选择符）。
// 汇编解析：mov %%fs,%%ax将fs寄存器的值传输到ax通用寄存器，
// "=a" (_v)表示将ax的值存入_v。
static inline unsigned long get_fs() 
{
	unsigned short _v;
	__asm__("mov %%fs,%%ax":"=a" (_v):);
	return _v;
}

// 功能：获取当前ds段寄存器的值（ds通常指向内核数据段）。
// 汇编解析：类似get_fs()，但操作的是ds段寄存器。
static inline unsigned long get_ds() 
{
	unsigned short _v;
	__asm__("mov %%ds,%%ax":"=a" (_v):);
	return _v;
}

// 功能：设置fs段寄存器的值（通常用于切换到用户空间的段选择符）。
// 汇编解析：mov %0,%%fs将%0（即val，转换为 16 位）写入fs寄存器，
// "a" (...)表示使用ax寄存器传递值。
static inline void set_fs(unsigned long val)
{
	__asm__("mov %0,%%fs"::"a" ((unsigned short) val));
}

// 总结
// 这些函数是早期 Linux 内核中用于内核态与用户态数据交互的核心工具：

// get_fs_xxx系列：从用户空间（fs段指向的内存）读取数据到内核。
// put_fs_xxx系列：从内核写入数据到用户空间（fs段指向的内存）。
// get_fs()/set_fs()：获取 / 设置fs段寄存器，用于切换内存访问的上下文（如从内核段切换到用户段）。

// 通过操作fs段寄存器，内核可以安全地访问用户空间的内存，这是系统调用、进程管理等功能的基础。