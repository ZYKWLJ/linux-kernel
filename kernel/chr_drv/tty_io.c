// tty 写函数
int tty_write(unsigned channel, char *buf, int nr)
{
    static int cr_flag = 0; // 回车标志
    struct tty_struct *tty;
    char c, *b = buf;

    if (channel > 2 || nr < 0) // 检查通道号和写入数量是否有效
        return -1;
    tty = channel + tty_table; // 获取对应的 tty 结构

    while (nr > 0) // 当还有字符要写入时循环
    {
        sleep_if_full(&tty->write_q); // 如果写队列满，则等待
        if (current->signal)          // 如果有信号
            break;

        // 当还有字符要写且写队列未满时循环
        while (nr > 0 && !FULL(tty->write_q))
        {
            c = get_fs_byte(b); // 从用户空间获取字符
            if (O_POST(tty))    // 如果设置了输出后处理
            {
                // CR 转 NL 处理
                if (c == '\r' && O_CRNL(tty))
                    c = '\n';
                // NL 转 CR 处理
                else if (c == '\n' && O_NLRET(tty))
                    c = '\r';
                // NL 转 CR-NL 处理
                if (c == '\n' && !cr_flag && O_NLCR(tty))
                {
                    cr_flag = 1;
                    PUTCH(13, tty->write_q); // 先写入 CR
                    continue;
                }
                // 小写转大写处理
                if (O_LCUC(tty))
                    c = toupper(c);
            }
            b++;                    // 移动缓冲区指针
            nr--;                   // 减少剩余要写入的字符数
            cr_flag = 0;            // 重置回车标志
            PUTCH(c, tty->write_q); // 将字符放入写队列
        }
        tty->write(tty); // 调用设备写函数
        if (nr > 0)      // 如果还有字符要写
            schedule();  // 调度其他进程运行
    }
    return (b - buf); // 返回实际写入的字符数
}

/*
 * 有时候我真的很喜欢 386。
 * 这个例程是从中断中调用的，
 * 即使在中断中睡眠也应该绝对没有问题（我希望）。
 * 当然，如果有人证明我错了，我会永远讨厌英特尔:-)。
 * 不过我们必须小心，在调用这个之前要恢复中断芯片。
 *
 * 我认为在正常情况下我们不会在这里睡眠，
 * 这很好，因为睡眠的任务可能是完全无辜的。
 */
// tty 中断处理函数
void do_tty_interrupt(int tty)
{
    copy_to_cooked(tty_table + tty); // 将原始数据转换为加工数据
}

// 字符设备初始化函数（空函数，保留用于未来扩展）
void chr_dev_init(void)
{
}