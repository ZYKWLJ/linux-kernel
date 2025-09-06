/*
 *  linux/fs/exec.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * #!-checking implemented by tytso.
 */

/*
 * 按需加载实现于91.12.01 - 只需要将头部读入内存。
 * 可执行文件的inode被放入"current->executable"，页故障负责实际加载。简洁。
 *
 * 我可以再次自豪地说linux经受住了更改：完全实现按需加载只用了不到2小时。
 */

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <a.out.h>

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/segment.h>

extern int sys_exit(int exit_code);
extern int sys_close(int fd);

/*
 * MAX_ARG_PAGES定义了为新程序的参数和环境变量分配的页数。
 * 32页应该足够，这给出了最大128kB的环境变量+参数空间！
 */
#define MAX_ARG_PAGES 32

/*
 * create_tables()解析新用户内存中的环境和参数字符串，
 * 并从中创建指针表，将它们的地址放在"栈"上，
 * 返回新的栈指针值。
 */
static unsigned long *create_tables(char *p, int argc, int envc)
{
    unsigned long *argv, *envp;
    unsigned long *sp;

    sp = (unsigned long *)(0xfffffffc & (unsigned long)p); // 栈指针4字节对齐
    sp -= envc + 1;                                        // 为环境指针数组预留空间（包括结束NULL）
    envp = sp;                                             // 环境指针数组起始位置
    sp -= argc + 1;                                        // 为参数指针数组预留空间（包括结束NULL）
    argv = sp;                                             // 参数指针数组起始位置

    // 按逆序压栈：envp, argv, argc
    put_fs_long((unsigned long)envp, --sp); // 环境指针数组地址
    put_fs_long((unsigned long)argv, --sp); // 参数指针数组地址
    put_fs_long((unsigned long)argc, --sp); // 参数个数

    // 填充参数指针数组
    while (argc-- > 0)
    {
        put_fs_long((unsigned long)p, argv++); // 设置参数字符串指针
        while (get_fs_byte(p++))               /* 跳过字符串直到遇到NULL */
            ;
    }
    put_fs_long(0, argv); // 参数数组以NULL结束

    // 填充环境变量指针数组
    while (envc-- > 0)
    {
        put_fs_long((unsigned long)p, envp++); // 设置环境变量字符串指针
        while (get_fs_byte(p++))               /* 跳过字符串直到遇到NULL */
            ;
    }
    put_fs_long(0, envp); // 环境变量数组以NULL结束

    return sp; // 返回新的栈顶指针
}

/*
 * count()计算参数/环境变量的数量
 */
static int count(char **argv)
{
    int i = 0;
    char **tmp;

    if ((tmp = argv))                                 // 如果指针数组不为空
        while (get_fs_long((unsigned long *)(tmp++))) // 遍历直到遇到NULL指针
            i++;

    return i; // 返回数量
}

/*
 * 'copy_string()'将参数/环境字符串从用户内存复制到内核内存的空闲页中。
 * 这些字符串的格式已经准备好可以直接放入新用户内存的顶部。
 *
 * 由TYT于91年11月24日修改，添加了from_kmem参数，该参数指定
 * 字符串和字符串数组是来自用户段还是内核段：
 *
 * from_kmem     argv *        argv **
 *    0          用户空间      用户空间
 *    1          内核空间      用户空间
 *    2          内核空间      内核空间
 *
 * 我们通过操作fs段寄存器来实现这一点。由于加载段寄存器的开销很大，
 * 我们尽量避免调用set_fs()，除非绝对必要。
 */
static unsigned long copy_strings(int argc, char **argv, unsigned long *page,
                                  unsigned long p, int from_kmem)
{
    char *tmp, *pag = NULL;
    int len, offset = 0;
    unsigned long old_fs, new_fs;

    if (!p)       // 如果p为0，直接返回
        return 0; /* 防止错误 */

    new_fs = get_ds(); // 内核数据段
    old_fs = get_fs(); // 当前fs值（通常是用户数据段）

    if (from_kmem == 2) // 如果字符串和指针数组都在内核空间
        set_fs(new_fs); // 设置fs为内核数据段

    while (argc-- > 0) // 遍历所有参数
    {
        if (from_kmem == 1) // 如果只有指针数组在用户空间
            set_fs(new_fs); // 临时设置fs为内核数据段

        // 获取参数字符串指针
        if (!(tmp = (char *)get_fs_long(((unsigned long *)argv) + argc)))
            panic("argc is wrong"); // 指针为空，参数计数错误

        if (from_kmem == 1) // 恢复fs为用户数据段
            set_fs(old_fs);

        len = 0; /* 记住要零填充 */
        // 计算字符串长度（包括结尾的NULL）
        do
        {
            len++;
        } while (get_fs_byte(tmp++));

        // 检查是否有足够空间
        if (p - len < 0)
        { /* 这不应该发生 - 128kB */
            set_fs(old_fs);
            return 0;
        }

        // 逆序复制字符串（从末尾开始）
        while (len)
        {
            --p;              // 前移目标指针
            --tmp;            // 前移源指针
            --len;            // 减少剩余长度
            if (--offset < 0) // 如果当前页已用完
            {
                offset = p % PAGE_SIZE; // 计算在新页中的偏移
                if (from_kmem == 2)     // 如果之前设置了内核fs
                    set_fs(old_fs);     // 临时恢复

                // 获取或分配新页
                if (!(pag = (char *)page[p / PAGE_SIZE]) &&
                    !(pag = (char *)(page[p / PAGE_SIZE] =
                                         get_free_page())))
                    return 0; // 分配失败

                if (from_kmem == 2) // 恢复内核fs
                    set_fs(new_fs);
            }
            // 复制一个字节
            *(pag + offset) = get_fs_byte(tmp);
        }
    }

    if (from_kmem == 2) // 恢复原来的fs值
        set_fs(old_fs);

    return p; // 返回新的位置指针
}

/*
 * change_ldt()修改LDT（局部描述符表），设置新的代码和数据段，
 * 并将参数页映射到数据段末尾。
 */
static unsigned long change_ldt(unsigned long text_size, unsigned long *page)
{
    unsigned long code_limit, data_limit, code_base, data_base;
    int i;

    // 代码段限制（页对齐）
    code_limit = text_size + PAGE_SIZE - 1;
    code_limit &= 0xFFFFF000;

    data_limit = 0x4000000; // 数据段限制64MB

    // 获取当前的段基址
    code_base = get_base(current->ldt[1]);
    data_base = code_base;

    // 设置新的段描述符
    set_base(current->ldt[1], code_base);   // 代码段基址
    set_limit(current->ldt[1], code_limit); // 代码段限制
    set_base(current->ldt[2], data_base);   // 数据段基址
    set_limit(current->ldt[2], data_limit); // 数据段限制

    /* 确保fs指向新的数据段 */
    __asm__("pushl $0x17\n\tpop %%fs" ::); // 0x17是数据段选择子

    data_base += data_limit; // 数据段末尾地址

    // 将参数页映射到数据段末尾（逆序）
    for (i = MAX_ARG_PAGES - 1; i >= 0; i--)
    {
        data_base -= PAGE_SIZE;           // 向前移动一页
        if (page[i])                      // 如果该页已分配
            put_page(page[i], data_base); // 映射页面
    }

    return data_limit; // 返回数据段限制
}

/*
 * 'do_execve()'执行一个新程序。
 * 这是execve系统调用的核心实现。
 */
int do_execve(unsigned long *eip, long tmp, char *filename,
              char **argv, char **envp)
{
    struct m_inode *inode;                           // 文件inode
    struct buffer_head *bh;                          // 缓冲区头
    struct exec ex;                                  // a.out头部信息
    unsigned long page[MAX_ARG_PAGES];               // 参数页数组
    int i, argc, envc;                               // 循环变量，参数计数，环境变量计数
    int e_uid, e_gid;                                // 有效用户ID和组ID
    int retval;                                      // 返回值
    int sh_bang = 0;                                 // shebang处理标志
    unsigned long p = PAGE_SIZE * MAX_ARG_PAGES - 4; // 参数空间起始位置

    // 检查是否从用户态调用（CS应为0x000f）
    if ((0xffff & eip[1]) != 0x000f)
        panic("execve called from supervisor mode");

    // 清空参数页表
    for (i = 0; i < MAX_ARG_PAGES; i++)
        page[i] = 0;

    // 获取可执行文件的inode
    if (!(inode = namei(filename)))
        return -ENOENT; // 文件不存在

    // 计算参数和环境变量的数量
    argc = count(argv);
    envc = count(envp);

// shebang处理重启点
restart_interp:
    // 检查文件类型，必须是普通文件
    if (!S_ISREG(inode->i_mode))
    {
        retval = -EACCES; // 不是普通文件，权限错误
        goto exec_error2;
    }

    // 计算有效用户ID和组ID（处理setuid/setgid）
    i = inode->i_mode;
    e_uid = (i & S_ISUID) ? inode->i_uid : current->euid;
    e_gid = (i & S_ISGID) ? inode->i_gid : current->egid;

    // 检查执行权限
    if (current->euid == inode->i_uid)      // 文件所有者
        i >>= 6;                            // 检查所有者权限位
    else if (current->egid == inode->i_gid) // 同组用户
        i >>= 3;                            // 检查组权限位

    // 其他用户的权限位已经在正确位置

    // 检查执行权限
    if (!(i & 1) && !((inode->i_mode & 0111) && suser()))
    {
        retval = -ENOEXEC; // 无执行权限
        goto exec_error2;
    }

    // 读取可执行文件的第一个块（包含头部）
    if (!(bh = bread(inode->i_dev, inode->i_zone[0])))
    {
        retval = -EACCES; // 读取失败
        goto exec_error2;
    }

    // 获取exec头部
    ex = *((struct exec *)bh->b_data);

    // 检查shebang（#!）脚本
    if ((bh->b_data[0] == '#') && (bh->b_data[1] == '!') && (!sh_bang))
    {
        /*
         * 处理shebang解释器行
         */
        char buf[1023], *cp, *interp, *i_name, *i_arg;
        unsigned long old_fs;

        // 复制shebang行（跳过"#!"）
        strncpy(buf, bh->b_data + 2, 1022);
        brelse(bh);       // 释放缓冲区
        iput(inode);      // 释放inode
        buf[1022] = '\0'; // 确保字符串终止

        // 查找换行符并截断
        if ((cp = strchr(buf, '\n')))
        {
            *cp = '\0';
            // 跳过前导空白字符
            for (cp = buf; (*cp == ' ') || (*cp == '\t'); cp++)
                ;
        }

        // 检查解释器名称是否存在
        if (!cp || *cp == '\0')
        {
            retval = -ENOEXEC; // 没有找到解释器名称
            goto exec_error1;
        }

        interp = i_name = cp;
        i_arg = 0;

        // 查找解释器路径中的基本名称（最后一个'/'之后的部分）
        for (; *cp && (*cp != ' ') && (*cp != '\t'); cp++)
        {
            if (*cp == '/')
                i_name = cp + 1;
        }

        // 分割解释器和参数
        if (*cp)
        {
            *cp++ = '\0';
            i_arg = cp;
        }

        /*
         * 已经解析出解释器名称和（可选的）参数
         */
        if (sh_bang++ == 0) // 第一次处理shebang
        {
            // 复制环境变量和参数（跳过原来的argv[0]）
            p = copy_strings(envc, envp, page, p, 0);
            p = copy_strings(--argc, argv + 1, page, p, 0);
        }

        /*
         * 按逆序拼接：
         * (1) shell脚本的文件名
         * (2) （可选的）解释器参数
         * (3) 解释器的名称（作为新的argv[0]）
         */
        p = copy_strings(1, &filename, page, p, 1); // 文件名
        argc++;
        if (i_arg) // 如果有解释器参数
        {
            p = copy_strings(1, &i_arg, page, p, 2); // 解释器参数
            argc++;
        }
        p = copy_strings(1, &i_name, page, p, 2); // 解释器名称
        argc++;

        if (!p) // 检查内存是否足够
        {
            retval = -ENOMEM;
            goto exec_error1;
        }

        /*
         * 使用解释器的inode重新启动进程
         */
        old_fs = get_fs();
        set_fs(get_ds());             // 设置内核数据段
        if (!(inode = namei(interp))) // 获取解释器的inode
        {
            set_fs(old_fs);
            retval = -ENOENT; // 解释器不存在
            goto exec_error1;
        }
        set_fs(old_fs);
        goto restart_interp; // 重新处理解释器
    }

    brelse(bh); // 释放缓冲区

    // 验证a.out格式
    if (N_MAGIC(ex) != ZMAGIC ||                                          // 魔数必须是ZMAGIC
        ex.a_trsize || ex.a_drsize ||                                     // 重定位信息必须为0
        ex.a_text + ex.a_data + ex.a_bss > 0x3000000 ||                   // 总大小不能超过48MB
        inode->i_size < ex.a_text + ex.a_data + ex.a_syms + N_TXTOFF(ex)) // 文件不能太小
    {
        retval = -ENOEXEC; // 格式错误
        goto exec_error2;
    }

    // 检查文本段偏移
    if (N_TXTOFF(ex) != BLOCK_SIZE)
    {
        printk("%s: N_TXTOFF != BLOCK_SIZE. See a.out.h.", filename);
        retval = -ENOEXEC;
        goto exec_error2;
    }

    // 如果不是shebang脚本，复制参数和环境变量
    if (!sh_bang)
    {
        p = copy_strings(envc, envp, page, p, 0); // 复制环境变量
        p = copy_strings(argc, argv, page, p, 0); // 复制参数
        if (!p)                                   // 检查内存是否足够
        {
            retval = -ENOMEM;
            goto exec_error2;
        }
    }

    /* 从这里开始是不可返回点 */

    // 更新当前进程的可执行文件信息
    if (current->executable)
        iput(current->executable);
    current->executable = inode;

    // 重置所有信号处理程序
    for (i = 0; i < 32; i++)
        current->sigaction[i].sa_handler = NULL;

    // 关闭标记为close_on_exec的文件
    for (i = 0; i < NR_OPEN; i++)
        if ((current->close_on_exec >> i) & 1)
            sys_close(i);
    current->close_on_exec = 0;

    // 释放旧的页表
    free_page_tables(get_base(current->ldt[1]), get_limit(0x0f)); // 代码段
    free_page_tables(get_base(current->ldt[2]), get_limit(0x17)); // 数据段

    // 清除数学协处理器状态
    if (last_task_used_math == current)
        last_task_used_math = NULL;
    current->used_math = 0;

    // 修改LDT并设置新的参数指针
    p += change_ldt(ex.a_text, page) - MAX_ARG_PAGES * PAGE_SIZE;
    p = (unsigned long)create_tables((char *)p, argc, envc); // 创建参数表

    // 设置内存边界
    current->brk = ex.a_bss +                                             // 程序break位置
                   (current->end_data = ex.a_data +                       // 数据段结束
                                        (current->end_code = ex.a_text)); // 代码段结束
    current->start_stack = p & 0xfffff000;                                // 栈起始地址（页对齐）

    // 设置有效用户ID和组ID
    current->euid = e_uid;
    current->egid = e_gid;

    // 在数据段末尾填充0（BSS段初始化）
    i = ex.a_text + ex.a_data;
    while (i & 0xfff) // 直到页边界
        put_fs_byte(0, (char *)(i++));

    // 设置新的执行上下文
    eip[0] = ex.a_entry; // 入口地址 -> EIP
    eip[3] = p;          // 栈指针 -> ESP

    return 0; // 成功（实际上不会返回到这里）

// 错误处理
exec_error2:
    iput(inode); // 释放inode
exec_error1:
    // 释放所有分配的参数页
    for (i = 0; i < MAX_ARG_PAGES; i++)
        free_page(page[i]);
    return (retval); // 返回错误码
}