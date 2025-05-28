#ifndef THREAD_INFO_H_
#define THREAD_INFO_H_
#include "include.h"
#include <time.h>
typedef unsigned int __u32;

typedef struct thread_info_
{
    /**
     * data descp: task_struct是 Linux 内核中描述进程的结构体。
     * 这里的task指针指向与该线程相关联的进程描述符，用于在内核中标识和管理线程所属的进程，
     * 方便获取进程相关信息，如进程状态、资源分配等 。
     */
    T_task_struct task;
    /**
     * data descp: exec_domain结构体用于表示可执行文件的执行域相关信息，
     * 比如不同操作系统或运行环境下可执行文件的加载和执行规则等。
     * 此指针指向与该线程执行相关的执行域描述符 。
     */
    E_exec_domain exec_domain;
    /**
     * data descp: flags用于存储该线程的一些标志位，这些标志位可以用来表示线程的各种属性和状态，
     * 如是否可抢占、是否处于特定运行模式等，通过不同的位组合来传达不同的信息 。
     */
    __u32 flags;
    /**
     * data descp: status用于记录线程的状态信息，比如运行状态、挂起状态等，
     * 具体含义会根据内核设计和实现有所不同 。
     */
    __u32 status;
    /**
     * data descp: 用来记录该线程当前运行在哪一个 CPU 上（在多 CPU 系统中），
     * 有助于进行 CPU 相关的调度和管理操作
     */
    __u32 cpu;
    /**
     * data descp: preempt_count是抢占计数。它用于控制内核抢占机制，值为 0 时表示线程可以被抢占，
     * 非零值表示当前线程处于不可抢占状态（比如在临界区执行等情况） 。
     */
    int preempt_count;
    /**
     * data descp: mm_segment_t是用于表示内存地址范围的类型。addr_limit定义了该线程在访问内存时的地址限制，
     * 用于内存访问权限控制，区分`内核空间`和`用户空间`的内存访问边界 。
     */
    M_mm_segment_t addr_limit;
    /**
     * data descp: restart_block结构体用于存储系统调用重启相关的信息，
     * 当系统调用被信号中断后，内核可以利用这些信息来正确地重启系统调用 。
     */
    Re_restart_block restart_block;
    /**
     * data descp: 用于存储系统调用从用户态进入内核态后返回地址相关信息，
     * 帮助内核在系统调用结束后正确返回到用户态的合适位置 。
     */
    void *sysenter_return;
    /**
     * data descp: 用于记录用户空间访问错误相关的信息，比如在进行用户空间和内核空间数据交互时，
     * 如果出现访问错误，可通过该变量记录相关错误状态 。
     */
    int uaccess_err;
} *T_thread_info;

T_thread_info T_thread_info_init(T_thread_info thread_info);
void  print_thread_info(T_thread_info thread_info);
#endif