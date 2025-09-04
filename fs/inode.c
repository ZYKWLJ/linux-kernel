// 释放一个i节点（回写入设备）
void iput(struct m_inode *inode)
{
    if (!inode) // 如果i节点指针为空，则返回
        return;
    wait_on_inode(inode); // 等待i节点解锁
    if (!inode->i_count)  // 如果i节点引用计数为0，则出错
        panic("iput: trying to free free inode");

    // 如果是管道i节点
    if (inode->i_pipe)
    {
        wake_up(&inode->i_wait); // 唤醒等待该管道的进程
        if (--inode->i_count)    // 引用计数减1，如果仍不为0则返回
            return;
        free_page(inode->i_size); // 释放管道占用的内存页面
        inode->i_count = 0;       // 复位引用计数
        inode->i_dirt = 0;        // 复位修改标志
        inode->i_pipe = 0;        // 复位管道标志
        return;
    }

    // 如果没有设备，则引用计数减1后返回
    if (!inode->i_dev)
    {
        inode->i_count--;
        return;
    }

    // 如果是块设备文件i节点，则同步设备
    if (S_ISBLK(inode->i_mode))
    {
        sync_dev(inode->i_zone[0]); // 同步设备
        wait_on_inode(inode);       // 等待i节点解锁
    }

repeat:
    // 如果i节点引用计数大于1，则减1后返回
    if (inode->i_count > 1)
    {
        inode->i_count--;
        return;
    }

    // 如果i节点链接数为0，则释放该i节点对应文件占用的所有磁盘块
    if (!inode->i_nlinks)
    {
        truncate(inode);   // 释放文件占用的所有磁盘块
        free_inode(inode); // 释放该i节点
        return;
    }

    // 如果该i节点已修改，则写盘
    if (inode->i_dirt)
    {
        write_inode(inode);   // 写i节点（可能睡眠）
        wait_on_inode(inode); // 等待i节点解锁
        goto repeat;          // 重新判断（因为写盘可能导致状态变化）
    }

    inode->i_count--; // 引用计数减1
    return;
}

// 从i节点表中获取一个空闲i节点项
struct m_inode *get_empty_inode(void)
{
    struct m_inode *inode;
    static struct m_inode *last_inode = inode_table; // 静态变量，指向最后一个查找的i节点
    int i;

    do
    {
        inode = NULL;
        // 循环扫描整个i节点表
        for (i = NR_INODE; i; i--)
        {
            // 从last_inode后开始寻找（循环查找）
            if (++last_inode >= inode_table + NR_INODE)
                last_inode = inode_table; // 如果超出表头则循环
            // 如果该i节点引用计数为0，则记下该i节点
            if (!last_inode->i_count)
            {
                inode = last_inode;
                // 如果该i节点未修改且未上锁，则找到空闲i节点，退出循环
                if (!inode->i_dirt && !inode->i_lock)
                    break;
            }
        }

        // 如果没有找到空闲i节点，则显示所有i节点状态后死机
        if (!inode)
        {
            for (i = 0; i < NR_INODE; i++)
                printk("%04x: %6d\t", inode_table[i].i_dev,
                       inode_table[i].i_num);
            panic("No free inodes in mem");
        }

        wait_on_inode(inode); // 等待该i节点解锁（如果已上锁）
        // 如果该i节点已修改，则将内容写入设备，并再次等待
        while (inode->i_dirt)
        {
            write_inode(inode);
            wait_on_inode(inode);
        }
    } while (inode->i_count); // 如果i节点又被占用，则重新寻找

    // 找到空闲i节点项，则清零并设置引用计数为1，返回该i节点指针
    memset(inode, 0, sizeof(*inode));
    inode->i_count = 1;
    return inode;
}

// 获取管道i节点（返回为i节点指针，如果是NULL则失败）
struct m_inode *get_pipe_inode(void)
{
    struct m_inode *inode;

    if (!(inode = get_empty_inode())) // 获取空闲i节点
        return NULL;
    // 为管道分配一页内存作为缓冲区
    if (!(inode->i_size = get_free_page()))
    {
        inode->i_count = 0; // 失败则释放i节点
        return NULL;
    }
    inode->i_count = 2;                        // 引用计数设为2（读和写）
    PIPE_HEAD(*inode) = PIPE_TAIL(*inode) = 0; // 初始化管道头尾指针
    inode->i_pipe = 1;                         // 设置管道标志
    return inode;
}

// 从设备上读取指定节点号的i节点（设备号dev，节点号nr）
struct m_inode *iget(int dev, int nr)
{
    struct m_inode *inode, *empty;

    if (!dev) // 设备号为0则死机
        panic("iget with dev==0");

    // 从i节点表中取一个空闲i节点
    empty = get_empty_inode();
    inode = inode_table; // 指向i节点表头
    // 扫描整个i节点表
    while (inode < NR_INODE + inode_table)
    {
        // 如果当前i节点的设备号不等于指定设备号或者节点号不等于指定节点号，则继续
        if (inode->i_dev != dev || inode->i_num != nr)
        {
            inode++;
            continue;
        }
        wait_on_inode(inode); // 等待该i节点解锁
        // 由于进程睡眠可能发生变化，所以需要再次判断
        if (inode->i_dev != dev || inode->i_num != nr)
        {
            inode = inode_table; // 重新扫描整个i节点表
            continue;
        }
        inode->i_count++; // 增加该i节点的引用计数
        // 如果该i节点是其它文件系统的安装点，则寻找安装在此i节点的超级块
        if (inode->i_mount)
        {
            int i;
            // 查找安装在此i节点的超级块
            for (i = 0; i < NR_SUPER; i++)
                if (super_block[i].s_imount == inode)
                    break;
            // 如果没有找到对应的超级块，则显示警告信息
            if (i >= NR_SUPER)
            {
                printk("Mounted inode hasn't got sb\n");
                if (empty)
                    iput(empty); // 释放开始获取的空闲i节点
                return inode;    // 返回该i节点
            }
            iput(inode); // 释放该i节点
            // 将设备号设置为超级块所在的设备号
            dev = super_block[i].s_dev;
            // 节点号设置为根文件系统的根i节点号（ROOT_INO=1）
            nr = ROOT_INO;
            inode = inode_table; // 重新扫描i节点表
            continue;
        }
        if (empty)
            iput(empty); // 释放开始获取的空闲i节点
        return inode;    // 返回找到的i节点
    }

    // 如果没有找到指定的i节点，则利用前面获取的空闲i节点
    if (!empty)
        return (NULL); // 如果没有空闲节点，返回NULL

    inode = empty;      // 使用该空闲i节点
    inode->i_dev = dev; // 设置设备号
    inode->i_num = nr;  // 设置i节点号
    read_inode(inode);  // 从设备上读取该i节点信息
    return inode;       // 返回该i节点
}

// 从设备上读取指定i节点信息
static void read_inode(struct m_inode *inode)
{
    struct super_block *sb;
    struct buffer_head *bh;
    int block;

    lock_inode(inode); // 锁定该i节点
    // 取该设备的超级块
    if (!(sb = get_super(inode->i_dev)))
        panic("trying to read inode without dev");
    // 计算该i节点所在的磁盘块号
    block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks +
            (inode->i_num - 1) / INODES_PER_BLOCK;
    // 从设备上读取该磁盘块
    if (!(bh = bread(inode->i_dev, block)))
        panic("unable to read i-node block");
    // 将磁盘块中的指定i节点信息复制到内存i节点中
    *(struct d_inode *)inode =
        ((struct d_inode *)bh->b_data)
            [(inode->i_num - 1) % INODES_PER_BLOCK];
    brelse(bh);          // 释放磁盘块缓冲区
    unlock_inode(inode); // 解锁该i节点
}

// 将指定i节点信息写入设备（写入缓冲区）
static void write_inode(struct m_inode *inode)
{
    struct super_block *sb;
    struct buffer_head *bh;
    int block;

    lock_inode(inode); // 锁定该i节点
    // 如果该i节点没有被修改过，或者没有设备，则解锁后返回
    if (!inode->i_dirt || !inode->i_dev)
    {
        unlock_inode(inode);
        return;
    }
    // 取该设备的超级块
    if (!(sb = get_super(inode->i_dev)))
        panic("trying to write inode without device");
    // 计算该i节点所在的磁盘块号
    block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks +
            (inode->i_num - 1) / INODES_PER_BLOCK;
    // 从设备上读取该磁盘块
    if (!(bh = bread(inode->i_dev, block)))
        panic("unable to read i-node block");
    // 将内存i节点信息复制到磁盘块中的相应位置
    ((struct d_inode *)bh->b_data)
        [(inode->i_num - 1) % INODES_PER_BLOCK] =
            *(struct d_inode *)inode;
    bh->b_dirt = 1;      // 设置缓冲区已修改标志
    inode->i_dirt = 0;   // 清除i节点已修改标志
    brelse(bh);          // 释放磁盘块缓冲区
    unlock_inode(inode); // 解锁该i节点
}