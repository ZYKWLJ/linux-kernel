/*
 *  linux/kernel/traps.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'Traps.c' handles hardware traps and faults after we have saved some
 * state in 'asm.s'. Currently mostly a debugging-aid, will be extended
 * to mainly kill the offending process (probably by giving it a signal,
 * but possibly by killing it outright if necessary).
 */
#include <string.h> /*定义了字符操作的汇编、C嵌入式函数，string.h声明位于include/下，实现位于/lib/string.c*/

#include <linux/head.h>  /*head头文件，定义了段描述符的简单结构，和几个选择符常量*/
#include <linux/sched.h> /*sched调度程序头文件，定义了任务结构体task_struct、初始化任务0的数据*/
#include <linux/kernel.h>/*内核头文件。含有一些常见的内核常用函数的原型定义*/
#include <asm/system.h>  /*系统头文件，定义了设置、修改描述符、中断门等的嵌入式汇编函数*/
#include <asm/segment.h> /*段操作头文件。定义了有关段寄存器操作的嵌入式汇编函数。*/
#include <asm/io.h>      /*io头文件，定义了硬件端口输入输出的嵌入式汇编函数*/

/*这里保证原汁原味的源码，下面的嵌入式函数，就不分行写了*/

/*取地址addr处的字节*/
/*注意，这里的"0" 是 输入操作数的约束（constraint），表示 “使用与第 0 个输出操作数相同的寄存器”。即ax*/
/*"m"表示使用内存地址*/
#define get_seg_byte(seg, addr) ({ \
register char __res; \
__asm__("push %%fs;mov %%ax,%%fs;movb %%fs:%2,%%al;pop %%fs" \
	:"=a" (__res):"0" (seg),"m" (*(addr))); \
__res; })

/*取地址addr处的长字(4字节)*/

#define get_seg_long(seg, addr) ({ \
register unsigned long __res; \
__asm__("push %%fs;mov %%ax,%%fs;movl %%fs:%2,%%eax;pop %%fs" \
	:"=a" (__res):"0" (seg),"m" (*(addr))); \
__res; })

/*取出fs寄存器的值(选择符)*/
#define _fs() ({ \
register unsigned short __res; \
__asm__("mov %%fs,%%ax":"=a" (__res):); \
__res; })

/*定义的一些函数原型*/
int do_exit(long code); /*退出系统调用，实现在kernel/exit.c 102行*/

void page_exception(void); /*页异常处理，实际是page_fault，mm/page.s 14行*/

/*下面定义了中断处理程序原型，代码定义在kernel/asm.s或者system_call.s中。*/
void divide_error(void);                /*int 0(kernel/asm.s 19)*/
void debug(void);                       /*int 1(kernel/asm.s 53)*/
void nmi(void);                         /*int 2(kernel/asm.s 57)*/
void int3(void);                        /*int 3(kernel/asm.s 61)*/
void overflow(void);                    /*int 4(kernel/asm.s 65)*/
void bounds(void);                      /*int 5(kernel/asm.s 69)*/
void invalid_op(void);                  /*int 6(kernel/asm.s 73)*/
void device_not_available(void);        /*int 7 (kernel/system_call.s 148)*/
void double_fault(void);                /*int 8 kernel/asm.s 97*/
void coprocessor_segment_overrun(void); /*int 9(kernel/asm.s 77)*/
void invalid_TSS(void);                 /*int 10(kernel/asm.s 131)*/
void segment_not_present(void);         /*int 11(kernel/asm.s 135)*/
void stack_segment(void);               /*int 12(kernel/asm.s 139)*/
void general_protection(void);          /*int 13(kernel/asm.s 143)*/
void page_fault(void);                  /*int 14(mm/page.s 14)*/
void coprocessor_error(void);           /*int 16(kernel/system_call.s 131)*/
void reserved(void);                    /*int 15(kernel/asm.s 81)*/
void parallel_interrupt(void);          /*int 39(kernel/system_call.s 280)*/
void irq13(void);                       /*int 45协处理器中断处理 (kernel/asm.s 85)*/

/*该子程序用来打印出错中断的名称、出错号、调用程序的EIP、ESP、EFLAGS、FS段寄存器值、*/
// 段的基址、段的长度、进程号pid、任务号、10字节指令码。
/*如果堆栈在用户段，则还打印16字节的堆栈内容。*/

static void die(char *str, long esp_ptr, long nr)
{
    /*强制转化，使得可以下标访问*/
    long *esp = (long *)esp_ptr;
    int i;

    printk("%s: %04x\n\r", str, nr & 0xffff);
    /*esp[1]是段选择符，esp[0]是偏移量，esp[2]是EFLAGS，esp[4]是FS，esp[3]是ESP*/
    printk("EIP:\t%04x:%p\nEFLAGS:\t%p\nESP:\t%04x:%p\n",
           esp[1], esp[0], esp[2], esp[4], esp[3]);
    // 打印FS寄存器的值
    printk("fs: %04x\n", _fs());
    // 打印段描述符的基址和限长
    printk("base: %p, limit: %p\n", get_base(current->ldt[1]), get_limit(0x17));

    /*如果ESP在用户空间，打印堆栈内容，可是为什么这里是0x17呢？*/
    /*这里是检查段选择子，看是不是用户态,段选择子=0x17=00010111*/
    /*段选择子结构为：*/
    //     高13位           1位    2位
    // +---------------+-----+-----+
    // | 描述符索引     | TI  | RPL |
    // +---------------+-----+-----+
    // 在早期 x86 系统（如 Linux 0.0x/0.1x 内核、DOS 等）中，
    // 会为 “用户数据段” 分配固定的段选择子（如 0x17 ），方便内核快速识别 “用户态段”。

    if (esp[4] == 0x17)
    {
        printk("Stack: ");
        for (i = 0; i < 4; i++)
            printk("%p ", get_seg_long(0x17, i + (long *)esp[3]));
        printk("\n");
    }
    /*取出当前任务的任务号，位于include/linux/sched.h 159*/
    str(i);
    /*打印出进程的pid和任务号*/
    printk("Pid: %d, process nr: %d\n\r", current->pid, 0xffff & i);

    /*打印异常发生时 EIP 指针附近的 10 个字节内存数据（通常是指令代码），用于调试分析错误原因，最后触发进程退出。*/
    /*也就是打印异常时，CPU指向的附近10个指令，方便调试*/
    for (i = 0; i < 10; i++)
        printk("%02x ", 0xff & get_seg_byte(esp[1], (i + (char *)esp[0])));
    printk("\n\r");
    do_exit(11); /* play segment exception */
}

/**
 * func descp: 以下这些以 do_开头的函数是对应名称中断处理程序调用的c函数。
 */
void do_double_fault(long esp, long error_code)
{
    die("double fault", esp, error_code);
}

void do_general_protection(long esp, long error_code)
{
    die("general protection", esp, error_code);
}

void do_divide_error(long esp, long error_code)
{
    die("divide error", esp, error_code);
}

/*do_int3用于在程序触发断点指令时，打印当前 CPU 寄存器状态和关键系统信息，辅助调试。*/
void do_int3(long *esp, long error_code,
             long fs, long es, long ds,
             long ebp, long esi, long edi,
             long edx, long ecx, long ebx, long eax)
{
    int tr;

    __asm__("str %%ax" : "=a"(tr) : "0"(0));
    printk("eax\t\tebx\t\tecx\t\tedx\n\r%8x\t%8x\t%8x\t%8x\n\r",
           eax, ebx, ecx, edx);
    printk("esi\t\tedi\t\tebp\t\tesp\n\r%8x\t%8x\t%8x\t%8x\n\r",
           esi, edi, ebp, (long)esp);
    printk("\n\rds\tes\tfs\ttr\n\r%4x\t%4x\t%4x\t%4x\n\r",
           ds, es, fs, tr);
    printk("EIP: %8x   CS: %4x  EFLAGS: %8x\n\r", esp[0], esp[1], esp[2]);
}

void do_nmi(long esp, long error_code)
{
    die("nmi", esp, error_code);
}

void do_debug(long esp, long error_code)
{
    die("debug", esp, error_code);
}

void do_overflow(long esp, long error_code)
{
    die("overflow", esp, error_code);
}

void do_bounds(long esp, long error_code)
{
    die("bounds", esp, error_code);
}

void do_invalid_op(long esp, long error_code)
{
    die("invalid operand", esp, error_code);
}

void do_device_not_available(long esp, long error_code)
{
    die("device not available", esp, error_code);
}

void do_coprocessor_segment_overrun(long esp, long error_code)
{
    die("coprocessor segment overrun", esp, error_code);
}

void do_invalid_TSS(long esp, long error_code)
{
    die("invalid TSS", esp, error_code);
}

void do_segment_not_present(long esp, long error_code)
{
    die("segment not present", esp, error_code);
}

void do_stack_segment(long esp, long error_code)
{
    die("stack segment", esp, error_code);
}

void do_coprocessor_error(long esp, long error_code)
{
    if (last_task_used_math != current)
        return;
    die("coprocessor error", esp, error_code);
}

void do_reserved(long esp, long error_code)
{
    die("reserved (15,17-47) error", esp, error_code);
}

// void trap_init(void) 是操作系统内核初始化阶段的关键函数，用于设置中断向量表（IDT，中断描述符表），
// 将各种硬件 / 软件异常（trap）和中断（interrupt）与对应的处理函数关联起来。
void trap_init(void)
{
    int i;
    /**
     * data descp: 异常处理函数注册（0~16 号向量）
     */
    set_trap_gate(0, &divide_error); // 0号：除法错误（如除以0）
    set_trap_gate(1, &debug);        // 1号：调试异常
    set_trap_gate(2, &nmi);          // 2号：非屏蔽中断（NMI，不可屏蔽的硬件中断）
    /*3~5号中断可以被任意内核态、用户态调用*/
    set_system_gate(3, &int3);                      // 3号：int3断点中断（调试用）
    set_system_gate(4, &overflow);                  // 4号：溢出异常
    set_system_gate(5, &bounds);                    // 5号：数组越界异常
    set_trap_gate(6, &invalid_op);                  // 6号：无效指令异常
    set_trap_gate(7, &device_not_available);        // 7号：设备不可用（如协处理器未就绪）
    set_trap_gate(8, &double_fault);                // 8号：双重故障（严重错误，通常导致系统崩溃）
    set_trap_gate(9, &coprocessor_segment_overrun); // 9号：协处理器段溢出
    set_trap_gate(10, &invalid_TSS);                // 10号：无效TSS（任务状态段错误）
    set_trap_gate(11, &segment_not_present);        // 11号：段不存在（访问了未加载的段）
    set_trap_gate(12, &stack_segment);              // 12号：栈段错误（栈溢出或权限问题）
    set_trap_gate(13, &general_protection);         // 13号：通用保护错误（如权限越界）
    set_trap_gate(14, &page_fault);                 // 14号：页错误（虚拟内存未映射物理内存）
    set_trap_gate(15, &reserved);                   // 15号：保留（未使用）
    set_trap_gate(16, &coprocessor_error);          // 16号：协处理器错误（如浮点运算错误）

    /**
     * data descp: 早期内核中，17~47 号向量可能对应硬件中断（IRQ）或未使用的预留向量，
     * 暂时统一用 reserved 函数处理（通常打印 “未实现” 信息）。
     */
    for (i = 17; i < 48; i++)
        set_trap_gate(i, &reserved);
    // 45号：单独指定为irq13（协处理器中断）
    set_trap_gate(45, &irq13);
    // 配置主8259A PIC（中断控制器）
    outb_p(inb_p(0x21) & 0xfb, 0x21);
    // 配置从8259A PIC
    outb(inb_p(0xA1) & 0xdf, 0xA1);
    // 39号：并行端口（如打印机）中断处理
    set_trap_gate(39, &parallel_interrupt);
}
