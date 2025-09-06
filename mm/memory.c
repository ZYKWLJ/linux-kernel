/*
 * 尝试共享页面
 * 检查任务"p"中地址"address"处的页面是否存在且干净，如果是，与当前任务共享
 * 假设已检查p != current，并且它们共享相同的可执行文件
 */
static int try_to_share(unsigned long address, struct task_struct *p)
{
    unsigned long from;
    unsigned long to;
    unsigned long from_page;
    unsigned long to_page;
    unsigned long phys_addr;

    from_page = to_page = ((address >> 20) & 0xffc); // 计算页目录项偏移
    from_page += ((p->start_code >> 20) & 0xffc);    // 加上进程p的代码段基地址
    to_page += ((current->start_code >> 20) & 0xffc); // 加上当前进程的代码段基地址
    /* 在from处有页目录吗？ */
    from = *(unsigned long *)from_page; // 获取页目录项
    if (!(from & 1))          // 如果页目录项无效
        return 0;
    from &= 0xfffff000;       // 获取页表地址
    from_page = from + ((address >> 10) & 0xffc); // 计算页表项地址
    phys_addr = *(unsigned long *)from_page; // 获取页表项（物理地址和标志）
    /* 页面是否干净且存在？ */
    if ((phys_addr & 0x41) != 0x01) // 检查存在位和脏位（应该是存在且干净）
        return 0;
    phys_addr &= 0xfffff000;  // 获取物理页面地址
    if (phys_addr >= HIGH_MEMORY || phys_addr < LOW_MEM) // 检查物理地址有效性
        return 0;
    to = *(unsigned long *)to_page; // 获取目标页目录项
    if (!(to & 1))          // 如果目标页目录项无效
    {
        if ((to = get_free_page())) // 分配新页表
            *(unsigned long *)to_page = to | 7; // 设置页目录项
        else
            oom();               // 内存不足
    }
    to &= 0xfffff000;       // 获取页表地址
    to_page = to + ((address >> 10) & 0xffc); // 计算目标页表项地址
    if (1 & *(unsigned long *)to_page) // 如果目标页表项已存在
        panic("try_to_share: to_page already exists");
    /* 共享它们：写保护 */
    *(unsigned long *)from_page &= ~2; // 清除源页表项的写标志
    *(unsigned long *)to_page = *(unsigned long *)from_page; // 复制页表项
    invalidate();            // 刷新TLB
    phys_addr -= LOW_MEM;    // 计算物理页面索引
    phys_addr >>= 12;
    mem_map[phys_addr]++;    // 增加页面引用计数
    return 1;
}

/*
 * 共享页面
 * 尝试找到可以与当前进程共享页面的进程
 * address是相对于当前数据空间的所需页面的地址
 * 首先通过检查executable->i_count检查是否可行
 * 如果有其他任务共享此inode，它应该>1
 */
static int share_page(unsigned long address)
{
    struct task_struct **p;

    if (!current->executable) // 如果没有可执行文件
        return 0;
    if (current->executable->i_count < 2) // 如果只有当前进程使用该可执行文件
        return 0;
    for (p = &LAST_TASK; p > &FIRST_TASK; --p) // 遍历所有任务
    {
        if (!*p)             // 如果任务槽为空
            continue;
        if (current == *p)   // 跳过当前任务
            continue;
        if ((*p)->executable != current->executable) // 如果可执行文件不同
            continue;
        if (try_to_share(address, *p)) // 尝试共享页面
            return 1;
    }
    return 0;
}

/*
 * 处理缺页错误
 * 当访问的页面不存在时调用
 */
void do_no_page(unsigned long error_code, unsigned long address)
{
    int nr[4];                // 设备块号数组
    unsigned long tmp;
    unsigned long page;       // 页面地址
    int block, i;

    address &= 0xfffff000;    // 页面对齐地址
    tmp = address - current->start_code; // 计算在可执行文件中的偏移
    if (!current->executable || tmp >= current->end_data) // 如果超出数据段
    {
        get_empty_page(address); // 获取空页面
        return;
    }
    if (share_page(tmp))      // 尝试共享页面
        return;
    if (!(page = get_free_page())) // 分配新页面
        oom();
    /* 记住1个块用于头文件 */
    block = 1 + tmp / BLOCK_SIZE; // 计算在可执行文件中的块号
    for (i = 0; i < 4; block++, i++) // 获取4个连续块号（用于读取一页）
        nr[i] = bmap(current->executable, block);
    bread_page(page, current->executable->i_dev, nr); // 从设备读取页面
    i = tmp + 4096 - current->end_data; // 计算需要清零的字节数（超出数据段的部分）
    tmp = page + 4096;        // 指向页面末尾
    while (i-- > 0)           // 清零超出数据段的部分
    {
        tmp--;
        *(char *)tmp = 0;
    }
    if (put_page(page, address)) // 映射页面
        return;
    free_page(page);          // 如果映射失败，释放页面
    oom();                    // 内存不足处理
}

/*
 * 内存初始化
 * 设置内存映射数组，标记已使用和空闲的区域
 */
void mem_init(long start_mem, long end_mem)
{
    int i;

    HIGH_MEMORY = end_mem;    // 设置高端内存地址
    for (i = 0; i < PAGING_PAGES; i++) // 初始化所有页面为已使用
        mem_map[i] = USED;
    i = MAP_NR(start_mem);    // 计算起始内存的页面索引
    end_mem -= start_mem;     // 计算可用内存大小
    end_mem >>= 12;           // 转换为页面数
    while (end_mem-- > 0)     // 标记可用页面为空闲
        mem_map[i++] = 0;
}

/*
 * 计算内存使用情况
 * 打印空闲页面数和各页目录使用的页面数
 */
void calc_mem(void)
{
    int i, j, k, free = 0;
    long *pg_tbl;

    for (i = 0; i < PAGING_PAGES; i++) // 统计空闲页面数
        if (!mem_map[i])
            free++;
    printk("%d pages free (of %d)\n\r", free, PAGING_PAGES); // 打印空闲页面信息
    for (i = 2; i < 1024; i++) // 遍历页目录（跳过前两项，内核空间）
    {
        if (1 & pg_dir[i])    // 如果页目录项有效
        {
            pg_tbl = (long *)(0xfffff000 & pg_dir[i]); // 获取页表地址
            for (j = k = 0; j < 1024; j++) // 统计页表中有效的页表项数
                if (pg_tbl[j] & 1)
                    k++;
            printk("Pg-dir[%d] uses %d pages\n", i, k); // 打印页目录项使用情况
        }
    }
}