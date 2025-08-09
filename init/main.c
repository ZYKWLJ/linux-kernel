/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */
/*
_LIBRARY__ 是一个特殊的编译时标志，用于指示编译器在包含 unistd.h 头文件时启用特定的代码路径。
当定义了 __LIBRARY__ 时，unistd.h 头文件会`包含系统调用的底层实现`，这些实现通常使用`内嵌汇编代码`直接与`内核`交互。

实际效果：当你定义了 __LIBRARY__ 并包含 unistd.h 后，头文件中的系统调用函数
(如 read()、write()、time() 等)会被展开为直接调用内核的汇编代码，而不是使用标准 C 库的封装。
这种方式通常用于需要`高效执行系统调用`的场景，或者在内核编程中使用。
*/
#define __LIBRARY__ /*定义该变量是为了包括定义在unistd.h中的内嵌汇编代码等信息*/
/*
*.h头文件所在的默认目录是 include/,则在代码中就`不用明确指明位置`。如果不是UNIX 的`标准头文件`,则需要指明所在的目录,
并用双引号括住。
*/
/*标准符号常数与类型文件:该文件中定义了`各种符号常数和类型,`并`声明了各种函数`。如果定义了LIBRARY,则还包括系统调用号和内嵌汇编syscall0()等*/
#include <unistd.h>
/*时间类型头文件。其中最主要定义了`tm 结构`和一些有关时间的函数原型。*/
#include <time.h>

/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */

/*我们需要下面这些语句内嵌，从内核空间创建进程`将导致没有写时复制`，直到执行一个execve调用。
这对堆栈可能带来问题。处理方法是在fork调用之后不让main使用堆栈——因此就不能有函数调用——这也意味着fork需要内存区代码
否则再从frok退出时就要使用堆栈了。

实际上`只有pause和fork需要内嵌方式`，以保证从main()中不会弄乱堆栈，但是我们同时还定义了其他一些内嵌宏函数(一致性保证)！
*/

/*下面的代码为什么不需要`;`？因为下面四个语句全是unistd.h里面的宏替换，会调换成系统调用！*/
/*例如下面这一句会文本替换为：
#define _syscall0(type,name) \
  type name(void) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "0" (__NR_##name)); \
if (__res >= 0) \
    return (type) __res; \
errno = -__res; \
return -1; \
}
从而实现了完整的系统调用！
*/

/*sysycall0以嵌入汇编的形式调用Linux的系统调用中断0x80。该中断是所有系统调用的入口！
static inline _syscall0(int, fork) 这条语句表示int fork()创建进程系统调用。
_syscall0名称中最后的0表示无参数，1表示有1个参数。
*/
static inline _syscall0(int, fork)
    /*pause()系统调用，暂停进程的执行，直到收到一个信号。*/
    static inline _syscall0(int, pause)
    /*int setup(void* BIOS)系统调用*/
    static inline _syscall1(int, setup, void *, BIOS)
    /*int sync()系统调用：更新文件系统*/
    static inline _syscall0(int, sync)

#include <linux/tty.h> /*定义了有关tty_io，串行通信方面的参数、常数*/
/*定义调度头文件。里面定义了任务结构task_struct、第一个初始任务的数据，
还有一些以宏的形式定义的有关描述符参数设置和获取的嵌入式汇编函数程序*/
#include <linux/sched.h>
/*head头文件。定义了段描述符的简单结构，和几个选择符常量*/
#include <linux/head.h>
/*系统头文件。以宏的形式定义了许多有关设置或修改描述符/中断门等的嵌入式汇编子程序*/
#include <asm/system.h>
/*io头文件。以宏的嵌入式汇编程序形式定义对io端口操作的函数*/
#include <asm/io.h>
/*标准定义头文件。定义了NULL，offsetof(TYPE,MEMBER)。*/
#include <stddef.h>
/*标准参数头文件。以宏的形式定义变量参数列表。主要说明了一个类型(va_list)和三个宏(va_start,va_arg和va_end)
以及函数vsprintf、vprintf、vfprintf。
*/
#include <stdarg.h>
/*上面出现过一次了，这里为什么还要出现？*/
#include <unistd.h>
/*文件控制头文件。用于文件及其描述符的操作控制常数符号的定义。*/
#include <fcntl.h>
/*类型头文件。定义了基本的系统数据类型。*/
#include <sys/types.h>
/*文件系统头文件。定义文件表结构(flie,buffer_head,m_inode等)*/
#include <linux/fs.h>
    /*静态字符串数组，用作内核显示信息的缓存。*/
    static char printbuf[1024];
/*送格式化输出到一字符串中(在kernel/vsprintf.c)*/
extern int vsprintf();
/*函数原型初始化，本文件最后实现*/
extern void init(void);
/*块设备初始化子程序(在blk/drv/ll_rw_blk.c)*/
extern void blk_dev_init(void);
/*字符设备初始化(chr_drv/tty_io.c)*/
extern void chr_dev_init(void);
/*硬盘设备初始化程序(blk_drv/hd.c)*/
extern void hd_init(void);
/*软驱初始化程序(blk_drv/floppy.c)*/
extern void floppy_init(void);
/*内存管理初始化(mm/memory.c)*/
extern void mem_init(long start, long end);
/*虚拟盘初始化(blk_drv/ramdisk.c)*/
extern long rd_init(long mem_start, int length);
/*建立内核时间(秒)*/
extern long kernel_mktime(struct tm *tm);
/*内核启动时间(开机时间)(秒)*/
extern long startup_time;

/*
 * This is set up by the setup-routine at boot-time
 */
/*下面这些数据是由setup.s程序在引导时设置的*/
#define EXT_MEM_K (*(unsigned short *)0x90002)     /*1MB以后得扩展内存大小*/
#define DRIVE_INFO (*(struct drive_info *)0x90080) /*硬盘参数表基址*/
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC) /*根文件系统所在设备号*/

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */
// 下面这段程序很差劲,但我不知道如何正确实现,而且好像它还能运行。如果有关于实时时钟更多的资料,那我很感兴趣。
// 这些都是试探出来的,另外还看了一些BIOS程序

/*这段宏读取CMOS实时时钟信息。*/
/*0x70是写端口，0x80|addr是要读取的CNOS内存地址*/
/*0x71是读端口*/
#define CMOS_READ(addr) ({     \ 
    outb_p(0x80 | addr, 0x70); \ 
inb_p(0x71); \ 
})

/*BCD码转化为数字。*/
#define BCD_TO_BIN(val) ((val) = ((val) & 15) + ((val) >> 4) * 10)

/*该子程序读取CMOS时钟，并设置开机时间start_time(秒)*/
static void time_init(void)
{
    struct tm time;
    /*下面的循环用于控制时间误差在1s以内*/
    do
    {
        time.tm_sec = CMOS_READ(0); /*具体可见CMOS内存列表*/
        time.tm_min = CMOS_READ(2);
        time.tm_hour = CMOS_READ(4);
        time.tm_mday = CMOS_READ(7);
        time.tm_mon = CMOS_READ(8);
        time.tm_year = CMOS_READ(9);
    } while (time.tm_sec != CMOS_READ(0));
    BCD_TO_BIN(time.tm_sec);
    BCD_TO_BIN(time.tm_min);
    BCD_TO_BIN(time.tm_hour);
    BCD_TO_BIN(time.tm_mday);
    BCD_TO_BIN(time.tm_mon);
    BCD_TO_BIN(time.tm_year);
    time.tm_mon--;
    startup_time = kernel_mktime(&time);
}

/*机器具有的内存(字节数)*/
static long memory_end = 0;
/*高速缓冲区末端地址*/
static long buffer_memory_end = 0;
/*主内存(将用于分页)开始的位置*/
static long main_memory_start = 0;

/*用于存放硬盘参数表信息的结构体*/
struct drive_info
{
    char dummy[32];
} drive_info;

/**
 * func descp: 内核初始化程序！
 */
/*这个main函数就是任务0，是所有的进程的祖先！*/
void main(void) /* This really IS void, no error here. */
{               /* The startup routine assumes (well, ...) this */
                /*
                 * Interrupts are still disabled. Do necessary setups, then
                 * enable them
                 */

    ROOT_DEV = ORIG_ROOT_DEV;                   /*根设备号*/
    drive_info = DRIVE_INFO;                    /*机器内存数量*/
    memory_end = (1 << 20) + (EXT_MEM_K << 10); /*内存大小=1MB+扩展内存(KB)*1024*/
    memory_end &= 0xfffff000;                   /*忽略不到4KB(1页)的内存数*/
    if (memory_end > 16 * 1024 * 1024)          /*如果内存超过16MB，则按16MB计。*/
        memory_end = 16 * 1024 * 1024;
    if (memory_end > 12 * 1024 * 1024) /*如果内存：12~16MB，则设置缓冲区末端=4MB*/
        buffer_memory_end = 4 * 1024 * 1024;
    else if (memory_end > 6 * 1024 * 1024) /*如果内存：6~12MB，则设置缓冲区末端=2MB*/
        buffer_memory_end = 2 * 1024 * 1024;
    else
        buffer_memory_end = 1 * 1024 * 1024; /*否则设置缓冲区末端=1MB*/
    main_memory_start = buffer_memory_end;   /*主内存起始位置=缓冲区末端*/
#ifdef RAMDISK                               /*如果定义了虚拟盘，则初始化虚拟盘。此时主内存减少*/
    main_memory_start += rd_init(main_memory_start, RAMDISK * 1024);
#endif
    /**
     * data descp: 下面就是内核进行的所有初始化工作。
     */
    /*内存初始化工作，在mm/memory.c里*/
    mem_init(main_memory_start, memory_end);
    /*中断初始化，在kernel/traps.c里*/
    trap_init();
    /*块设备初始化，在kernel/blk_drv/ll_rw_blk.c*/
    blk_dev_init();
    /*字符设备初始化，在kernel/chr_drv/tty_io.c*/
    chr_dev_init();
    /*串口初始化，在kernel/chr_drv/tty_io.c*/
    tty_init();
    /*时钟初始化，在本文件中*/
    time_init();
    /*调度初始化，在kernel/sched.c中*/
    sched_init();
    /*高速缓冲区初始化，在fs/buffer.c中*/
    buffer_init(buffer_memory_end);
    /*硬盘初始化，在kernel/blk_drv/hd.c中*/
    hd_init();
    /*软盘初始化，在kernel/blk_drv/floppy.c中*/
    floppy_init();
    /*所有初始化工作都已经完成了，开启中断！在include/asm/system.h*/
    sti(); /*即start interupt*/
    /*移动到用户态，在include/asm/system.h*/
    move_to_user_mode();

    /*这里就是任务0开始创建子进程了！*/
    if (!fork()) /*我们全靠它了！*/
    {            /* we count on this going ok */
        /*函数初始化*/
        init(); /*在新建的子进程(任务1中执行init)*/
    }
    /*下面的代码开始以任务0的身份运行*/
    /*
     *   NOTE!!   For any other task 'pause()' would mean we have to get a
     * signal to awaken, but task0 is the sole exception (see 'schedule()')
     * as task 0 gets activated at every idle moment (when no other tasks
     * can run). For task0 'pause()' just means we go check if some other
     * task can run, and if not we return here.
     */
    /*注意，对于任何其他任务，`pause()`将意味着我们必须`等待收到一个信号`才会返回就绪`运行态`,但任务0(task0)是唯一的例外情况
    ，因为`任务0`在`任何空闲时间`里都会被`激活`(当没有其他任务在运行时)，因为对于任务0`pause()`,仅意味着我们返回来查看是否有其他任务
    可以运行,如果没有,则返回这里，一直循环执行`pause()`
    */

    for (;;)
        pause();
}
/*格式化信息输出到标准输出设备stdout(1),这里是指在屏幕上显示。
该程序就是vsprintf如何使用的一个例子*/
static int printf(const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    /*在kernel/vsprintf.c中*/
    write(1, printbuf, i = vsprintf(printbuf, fmt, args));
    va_end(args);
    return i;
}
/*用途：用于执行 /etc/rc 脚本，这是系统启动时的第一个 shell 脚本，负责初始化系统环境。*/
/*调用执行程序时参数的字符串数组*/
static char *argv_rc[] = {"/bin/sh", NULL};
/*调用执行程序时的环境字符串数组*/
static char *envp_rc[] = {"HOME=/", NULL};

/*用途：用于启动系统的交互式 shell，为用户提供登录后的命令行环境。*/
/*同上*/
static char *argv[] = {"-/bin/sh", NULL};
static char *envp[] = {"HOME=/usr/root", NULL};
/**
* func descp:
init()函数运行在任务0创建的`子进程(任务1)中`。他首先`对第一个要执行的程序(shell)的`环境`进行初始化`，
然后`加载该程序并执行之`！
*/
void init(void)
{
    int pid, i;
    /*读取硬盘参数包括分区表信息并建立虚拟盘和安装根文件系统设备。对应的函数是sys_setup()*/
    setup((void *)&drive_info);
    /*用读写访问方式打开设备/dev/tty0,这里对应终端控制台。返回的句柄0号——stdin标准输入设备。*/
    (void)open("/dev/tty0", O_RDWR, 0);
    /*复制句柄，产生句柄1号——是stdout标准输出设备*/
    (void)dup(0);
    /*复制句柄，产生句柄2号——是stderr标准出错输出设备(从这里看出，都是一样的)*/
    (void)dup(0);
    printf("%d buffers = %d bytes buffer space\n\r", NR_BUFFERS,
           NR_BUFFERS * BLOCK_SIZE);                                  /*打印缓冲区块数和总字节数,每块1024字节*/
    printf("Free mem: %d bytes\n\r", memory_end - main_memory_start); /*空闲内存字节数*/
    /**
     * data descp: 下面的fork用于创建一个子进程(子任务)。对于被创建的子进程，fork()将返回0值，对于原进程(父进程)将返回子进程的进程号。
     * 因此，下面的if语句就是判断是否为子进程。如果是子进程，就执行下面的代码。
     * 下面的代码就是用于执行 /etc/rc 脚本，这是系统启动时的第一个 shell 脚本，负责初始化系统环境。
     */
    /*如果是子进程*/
    /*关闭句柄0，以只读方式打开/etc/rc文件，并执行/bin/shell程序，所带参数和环境变量分别由argv_rc和envp_rc数组给出*/
    if (!(pid = fork()))
    {
        close(0);
        if (open("/etc/rc", O_RDONLY, 0))
            _exit(1);                        /*打开文件失败，则退出*/
        execve("/bin/sh", argv_rc, envp_rc); /*装入/bin/sh程序并执行*/
        _exit(2);                            /*若execve执行失败则退出*/
    }
    /*如果是父进程*/
    /*wait等待子进程停止或终止，其返回值是子进程的进程号(PID)。
    这三句的作用是父进程等待子进程的结束。&i是存放返回状态信息的位置。如果wait()返回值不等于子进程号，则继续等待。
    */
    if (pid > 0)
        while (pid != wait(&i))
            /* nothing */;
    /*
    如果执行到这里,说明刚创建的子进程的执行已停止或终止了。
    下面循环中首先再创建一个子进程,如果出错,则显示“初始化程序创建子进程失败”的信息并继续执行。
    对于所创建的子进程关闭所有以前还遗留的句柄(`stain,stdout,stderr`),新创建一个会话并设置进程组号,然后重新打开/ev/tty0作为 stdin,并复制成stdout 和stderr。
    再次执行系统解释程序/bin/sh。但这次执行所选用的参数和环境数组另选了argv[]、envp[]。然后父进程再次运行 wait()等待。
    如果子进程又停止了执行,则在标准输出上显示出错信息“子进程pid停止了运行,返回码是i”,然后继续重试下去。。。。。。
    这样形成一个大循环！
    */

    /*
    这个无限循环负责系统初始化后的`第一个用户进程`，并确保`系统始终`有一个 shell 可用。
    init 进程会不断`创建子进程运行 shell`，即使`子进程`意外终止，也会立即`创建新的 shell 进程`，确保系统可用性。
    */
    while (1)
    {
        /*创建子进程*/
        if ((pid = fork()) < 0)
        {
            printf("Fork failed in init\r\n");
            continue;
        }
        // 子进程逻辑
        if (!pid)
        {
            // 关闭标准输入、输出、错误
            close(0);
            close(1);
            close(2);

            // 创建新会话，脱离控制终端
            setsid();

            // 重新打开控制终端作为标准输入
            (void)open("/dev/tty0", O_RDWR, 0);
            // 复制标准输入到标准输出
            (void)dup(0);
            // 复制标准输入到标准错误
            (void)dup(0);

            // 执行 shell 程序，替换当前进程
            _exit(execve("/bin/sh", argv, envp));
        }
        // 父进程逻辑
        while (1)
            if (pid == wait(&i))
                break; // 等待子进程结束

        // 打印子进程退出状态并`同步文件系统`
        printf("\n\rchild %d died with code %04x\n\r", pid, i);
        // sync() 确保文件系统数据同步到磁盘
        sync();
    }
    _exit(0); /* NOTE! _exit, not exit() */
}

/*
为什么使用 _exit() 而非 exit()？
    ◦ _exit() 直接终止进程，不执行清理操作，会更快！
    ◦ exit() 会执行 atexit 函数、刷新缓冲区等
    ◦ 在 execve() 失败时，直接终止进程更安全
*/