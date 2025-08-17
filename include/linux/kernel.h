/*
 * 'kernel.h' contains some often-used function prototypes etc
 */
// 作用：用来验证内存区域是不是有效的
// 检查从 addr 开始的 count 个字节的内存区域是否可访问
// （例如是否属于合法的地址空间、是否有正确的权限等）。
// 在早期内核中，常用于防止用户空间程序访问非法内存，是内存安全检查的重要函数。
void verify_area(void * addr,int count);

// 功能：内核紧急错误处理。
// 具体作用：当内核发生无法恢复的严重错误（如致命 bug、硬件故障等）时被调用。
// 会打印 str 指向的错误信息，然后停止系统运行（可能触发内核崩溃、禁止中断等），防止错误扩散。
void panic(const char * str);

// 功能：格式化输出到标准输出。
int printf(const char * fmt, ...);

// 功能：内核日志输出(内核的printf)。
int printk(const char * fmt, ...);

// 功能：向终端设备写入数据。
int tty_write(unsigned ch,char * buf,int count);

// 功能：动态内存分配。
void * malloc(unsigned int size);

// 功能：释放动态分配的内存。
void free_s(void * obj, int size);

// 功能：简化内存释放操作。
// 通过宏定义将 free(x) 转换为 free_s(x, 0)，提供更简洁的接口。
// 这里传递 0 作为 size，可能表示让函数自动处理内存块大小（如通过内存块头部的元数据获取）。
#define free(x) free_s((x), 0)

/*
 * This is defined as a macro, but at some point this might become a
 * real subroutine that sets a flag if it returns true (to do
 * BSD-style accounting where the process is flagged if it uses root
 * privs).  The implication of this is that you should do normal
 * permissions checks first, and check suser() last.
 */

// 功能：检查当前进程是否为超级用户（root）。
// 具体作用：current 是指向当前运行进程的结构体指针，euid 是有效用户 ID。
// 具体作用：判断当前进程的有效用户 ID 是否为 0，若为 0 则表示该进程为超级用户（root）。
#define suser() (current->euid == 0)