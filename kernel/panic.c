/*
 * 此函数在整个内核中（包括内存管理mm和文件系统fs模块）被使用
 * 用于表示发生了无法恢复的重大问题
 */
#define PANIC // 定义PANIC宏，预留作恐慌状态标识（当前版本未实际使用）

#include <linux/kernel.h> // 包含内核基础功能定义，如printk打印函数
#include <linux/sched.h>  // 包含进程调度相关定义，如current当前进程指针、task进程数组

// 声明sys_sync函数（实际返回类型为int，此处简化声明为void不影响功能）
// sys_sync作用：将文件系统缓存中的数据同步到磁盘，防止数据丢失
void sys_sync(void);

/*
 * 内核恐慌处理函数
 * 参数s：描述恐慌原因的错误信息字符串
 * 功能：当内核遇到无法恢复的严重错误时，输出错误信息并冻结系统
 */
void panic(const char *s)
{
    // 打印内核恐慌信息到控制台（类似用户态的printf，用于内核态输出）
    printk("Kernel panic: %s\n\r", s);

    // 判断当前进程是否为空闲进程（swapper task，进程数组中的第一个进程task[0]）
    if (current == task[0])
        // 若在空闲进程中发生恐慌，无需同步数据（空闲进程不处理文件操作）
        printk("In swapper task - not syncing\n\r");
    else
        // 若在其他业务进程中发生恐慌，调用sys_sync同步磁盘数据，减少数据丢失风险
        sys_sync();

    // 进入无限循环，冻结内核（停止所有操作，保留现场供调试）
    // 此时系统无法继续运行，避免错误扩散
    for (;;)
        ;
}