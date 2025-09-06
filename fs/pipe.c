/*
 *  linux/fs/pipe.c
 *
 *  (C) 1991  Linus Torvalds
 *
 *  功能说明：Linux 0.11 内核中管道（Pipe）机制的核心实现
 *  管道是进程间通信（IPC）的一种方式，提供单向的字节流传输，
 *  实现了"读-写"模式的进程间数据交换
 */
#include <signal.h>      // 信号处理相关定义（如SIGPIPE）
#include <linux/sched.h> // 进程调度相关结构（如task_struct、sleep_on等）
#include <linux/mm.h>    // 内存管理函数（如get_free_page获取空闲页）
#include <asm/segment.h> // 段操作函数（如put_fs_byte、get_fs_byte跨空间访问）
/**
 * @brief 从管道读取数据
 * @param inode 管道对应的索引节点，存储管道缓冲区信息
 * @param buf 用户空间缓冲区，用于接收读取的数据
 * @param count 期望读取的字节数
 * @return 实际读取的字节数（0表示管道关闭或无数据）
 */
int read_pipe(struct m_inode *inode, char *buf, int count)
{
    int chars;    // 当前读取的字节数
    int size;     // 管道中可读取的数据总量
    int read = 0; // 累计读取的总字节数
    // 循环读取直到满足期望字节数或管道无数据
    while (count > 0)
    {
        // 等待管道中有数据（PIPE_SIZE宏获取当前管道数据量）
        while (!(size = PIPE_SIZE(*inode)))
        {
            wake_up(&inode->i_wait); // 唤醒等待在该管道上的写进程
            // 检查是否还有写进程（i_count为2表示有一个读和一个写进程）
            if (inode->i_count != 2)
                return read;          // 无写进程，返回已读取字节数
            sleep_on(&inode->i_wait); // 睡眠等待，直到有数据写入
        }
        // 计算当前可从管道尾部读取的最大字节数（不超过一页）
        chars = PAGE_SIZE - PIPE_TAIL(*inode);
        // 限制读取量不超过剩余请求数
        if (chars > count)
            chars = count;
        // 限制读取量不超过管道中实际数据量
        if (chars > size)
            chars = size;
        // 更新剩余请求数和已读取总数
        count -= chars;
        read += chars;
        // 记录当前管道尾部位置，更新尾部指针（循环缓冲区）
        size = PIPE_TAIL(*inode);
        PIPE_TAIL(*inode) += chars;
        // 用与运算实现环形缓冲区（PAGE_SIZE必须是2的幂）
        PIPE_TAIL(*inode) &= (PAGE_SIZE - 1);
        // 将管道数据从内核缓冲区复制到用户空间缓冲区
        while (chars-- > 0)
            put_fs_byte(((char *)inode->i_size)[size++], buf++);
    }
    wake_up(&inode->i_wait); // 唤醒可能等待的写进程
    return read;
}

/**
 * @brief 向管道写入数据
 * @param inode 管道对应的索引节点，存储管道缓冲区信息
 * @param buf 用户空间缓冲区，提供待写入的数据
 * @param count 期望写入的字节数
 * @return 实际写入的字节数；若管道无读进程，返回-1并发送SIGPIPE信号
 */
int write_pipe(struct m_inode *inode, char *buf, int count)
{
    int chars;       // 当前写入的字节数
    int size;        // 管道中可写入的空闲空间
    int written = 0; // 累计写入的总字节数
    // 循环写入直到满足期望字节数或管道满
    while (count > 0)
    {
        // 等待管道有空闲空间（计算可写入的最大空间）
        while (!(size = (PAGE_SIZE - 1) - PIPE_SIZE(*inode)))
        {
            wake_up(&inode->i_wait); // 唤醒等待在该管道上的读进程
            // 检查是否还有读进程（i_count为2表示有读写进程）
            if (inode->i_count != 2)
            {
                // 无读进程，发送SIGPIPE信号给当前进程
                current->signal |= (1 << (SIGPIPE - 1));
                // 返回已写入字节数，若无则返回-1
                return written ? written : -1;
            }
            sleep_on(&inode->i_wait); // 睡眠等待，直到有空间
        }
        // 计算当前可写入管道头部的最大字节数（不超过一页）
        chars = PAGE_SIZE - PIPE_HEAD(*inode);
        // 限制写入量不超过剩余请求数
        if (chars > count)
            chars = count;
        // 限制写入量不超过管道空闲空间
        if (chars > size)
            chars = size;
        // 更新剩余请求数和已写入总数
        count -= chars;
        written += chars;
        // 记录当前管道头部位置，更新头部指针（循环缓冲区）
        size = PIPE_HEAD(*inode);
        PIPE_HEAD(*inode) += chars;
        // 用与运算实现环形缓冲区
        PIPE_HEAD(*inode) &= (PAGE_SIZE - 1);
        // 将用户空间数据复制到内核管道缓冲区
        while (chars-- > 0)
            ((char *)inode->i_size)[size++] = get_fs_byte(buf++);
    }
    wake_up(&inode->i_wait); // 唤醒可能等待的读进程
    return written;
}

/**
 * @brief 创建管道系统调用，生成一对文件描述符（读端和写端）
 * @param fildes 用户空间数组指针，用于存储返回的两个文件描述符
 * @return 0表示成功，-1表示失败
 */
int sys_pipe(unsigned long *fildes)
{
    struct m_inode *inode; // 管道对应的索引节点
    struct file *f[2];     // 文件结构数组（读端和写端）
    int fd[2];             // 文件描述符数组
    int i, j;
    // 步骤1：从文件表中分配两个空闲文件结构
    j = 0;
    for (i = 0; j < 2 && i < NR_FILE; i++)
        if (!file_table[i].f_count)
            (f[j++] = i + file_table)->f_count++; // 引用计数+1
    // 若只分配到一个文件结构，回滚释放
    if (j == 1)
        f[0]->f_count = 0;
    // 若未分配到两个文件结构，返回失败
    if (j < 2)
        return -1;
    // 步骤2：从当前进程的文件描述符表中分配两个空闲项
    j = 0;
    for (i = 0; j < 2 && i < NR_OPEN; i++)
        if (!current->filp[i])
        {
            current->filp[fd[j] = i] = f[j]; // 绑定文件描述符与文件结构
            j++;
        }
    // 若只分配到一个描述符，回滚释放
    if (j == 1)
        current->filp[fd[0]] = NULL;
    // 若未分配到两个描述符，释放文件结构并返回失败
    if (j < 2)
    {
        f[0]->f_count = f[1]->f_count = 0;
        return -1;
    }
    // 步骤3：创建管道索引节点（分配缓冲区等资源）
    if (!(inode = get_pipe_inode()))
    {
        // 创建失败，回滚释放所有资源
        current->filp[fd[0]] = current->filp[fd[1]] = NULL;
        f[0]->f_count = f[1]->f_count = 0;
        return -1;
    }
    // 步骤4：初始化文件结构
    f[0]->f_inode = f[1]->f_inode = inode; // 共享同一个索引节点
    f[0]->f_pos = f[1]->f_pos = 0;         // 读写位置初始化为0
    f[0]->f_mode = 1;                      // 读模式
    f[1]->f_mode = 2;                      // 写模式
    // 步骤5：将文件描述符返回给用户空间
    put_fs_long(fd[0], 0 + fildes); // 第一个元素存读端描述符
    put_fs_long(fd[1], 1 + fildes); // 第二个元素存写端描述符
    return 0;                       // 成功创建管道
}