/*
 *  linux/kernel/console.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *	console.c
 *
 * 该模块实现了控制台IO函数
 *	'void con_init(void)'
 *	'void con_write(struct tty_queue * queue)'
 * 希望这将是一个相当完整的VT102实现。
 *
 * 鸣谢John T Kohl提供的蜂鸣支持。
 */

/*
 *  注意!!! 我们有时会短暂禁用和启用中断
 * (为了在视频IO中写入一个字)，但这甚至对键盘
 * 中断也有效。我们知道在获取键盘中断时中断没有被启用，
 * 因为我们使用陷阱门。希望一切顺利。
 */

/*
 * 检查不同视频卡的代码主要由Galen Hunt编写，
 * <g-hunt@ee.utah.edu>
 */

#include <linux/sched.h> // 调度程序头文件，定义了任务结构task_struct、任务0数据等
#include <linux/tty.h>   // tty头文件，定义了tty_io，串行通信方面的参数、常数
#include <asm/io.h>      // io头文件，定义硬件端口输入/输出宏汇编语句
#include <asm/system.h>  // 系统头文件，定义了设置或修改描述符/中断门等的嵌入式汇编宏

/*
 * 这些由启动时的setup-routine设置：
 */

#define ORIG_X (*(unsigned char *)0x90000)                             // 原始光标列号
#define ORIG_Y (*(unsigned char *)0x90001)                             // 原始光标行号
#define ORIG_VIDEO_PAGE (*(unsigned short *)0x90004)                   // 原始视频页
#define ORIG_VIDEO_MODE ((*(unsigned short *)0x90006) & 0xff)          // 原始视频模式
#define ORIG_VIDEO_COLS (((*(unsigned short *)0x90006) & 0xff00) >> 8) // 原始视频列数
#define ORIG_VIDEO_LINES (25)                                          // 原始视频行数
#define ORIG_VIDEO_EGA_AX (*(unsigned short *)0x90008)                 // EGA AX寄存器值
#define ORIG_VIDEO_EGA_BX (*(unsigned short *)0x9000a)                 // EGA BX寄存器值
#define ORIG_VIDEO_EGA_CX (*(unsigned short *)0x9000c)                 // EGA CX寄存器值

#define VIDEO_TYPE_MDA 0x10  /* 单色文本显示器 */
#define VIDEO_TYPE_CGA 0x11  /* CGA显示器 */
#define VIDEO_TYPE_EGAM 0x20 /* 单色模式下的EGA/VGA */
#define VIDEO_TYPE_EGAC 0x21 /* 彩色模式下的EGA/VGA */

#define NPAR 16 // 参数最大数量

extern void keyboard_interrupt(void); // 键盘中断处理函数

static unsigned char video_type;        /* 正在使用的显示器类型 */
static unsigned long video_num_columns; /* 文本列数 */
static unsigned long video_size_row;    /* 每行字节数 */
static unsigned long video_num_lines;   /* 文本行数 */
static unsigned char video_page;        /* 初始视频页 */
static unsigned long video_mem_start;   /* 视频RAM起始地址 */
static unsigned long video_mem_end;     /* 视频RAM结束地址（大致） */
static unsigned short video_port_reg;   /* 视频寄存器选择端口 */
static unsigned short video_port_val;   /* 视频寄存器值端口 */
static unsigned short video_erase_char; /* 用于擦除的字符+属性 */

static unsigned long origin;          /* 用于EGA/VGA快速滚动 */
static unsigned long scr_end;         /* 用于EGA/VGA快速滚动 */
static unsigned long pos;             /* 当前光标位置 */
static unsigned long x, y;            /* 当前光标列、行位置 */
static unsigned long top, bottom;     /* 滚动区域顶行、底行 */
static unsigned long state = 0;       /* 转义序列处理状态 */
static unsigned long npar, par[NPAR]; /* 参数数量和参数数组 */
static unsigned long ques = 0;        /* 问号标志 */
static unsigned char attr = 0x07;     /* 当前字符属性 */

static void sysbeep(void); // 系统蜂鸣函数

/*
 * 这是终端对ESC-Z或csi0c查询的响应（= vt100响应）。
 */
#define RESPONSE "\033[?1;2c"

/* 注意！gotoxy认为x==video_num_columns是可以的 */
// 移动光标到指定位置
static inline void gotoxy(unsigned int new_x, unsigned int new_y)
{
    if (new_x > video_num_columns || new_y >= video_num_lines)
        return;
    x = new_x;
    y = new_y;
    pos = origin + y * video_size_row + (x << 1); // 计算内存位置
}

// 设置显示起始地址（用于硬件滚动）
static inline void set_origin(void)
{
    cli();                                                            // 关中断
    outb_p(12, video_port_reg);                                       // 选择起始地址高寄存器
    outb_p(0xff & ((origin - video_mem_start) >> 9), video_port_val); // 设置高字节
    outb_p(13, video_port_reg);                                       // 选择起始地址低寄存器
    outb_p(0xff & ((origin - video_mem_start) >> 1), video_port_val); // 设置低字节
    sti();                                                            // 开中断
}

// 向上滚动区域
static void scrup(void)
{
    if (video_type == VIDEO_TYPE_EGAC || video_type == VIDEO_TYPE_EGAM)
    {
        if (!top && bottom == video_num_lines) // 如果是整个屏幕
        {
            origin += video_size_row;    // 移动起始地址
            pos += video_size_row;       // 移动光标位置
            scr_end += video_size_row;   // 移动结束地址
            if (scr_end > video_mem_end) // 如果超出视频内存
            {
                // 将整个屏幕上移一行
                __asm__("cld\n\t"
                        "rep\n\t"
                        "movsl\n\t"
                        "movl video_num_columns,%1\n\t"
                        "rep\n\t"
                        "stosw" ::"a"(video_erase_char),
                        "c"((video_num_lines - 1) * video_num_columns >> 1),
                        "D"(video_mem_start),
                        "S"(origin));
                scr_end -= origin - video_mem_start;
                pos -= origin - video_mem_start;
                origin = video_mem_start;
            }
            else
            {
                // 清除新行
                __asm__("cld\n\t"
                        "rep\n\t"
                        "stosw" ::"a"(video_erase_char),
                        "c"(video_num_columns),
                        "D"(scr_end - video_size_row));
            }
            set_origin(); // 更新硬件起始地址
        }
        else // 如果是部分区域
        {
            // 上移指定区域
            __asm__("cld\n\t"
                    "rep\n\t"
                    "movsl\n\t"
                    "movl video_num_columns,%%ecx\n\t"
                    "rep\n\t"
                    "stosw" ::"a"(video_erase_char),
                    "c"((bottom - top - 1) * video_num_columns >> 1),
                    "D"(origin + video_size_row * top),
                    "S"(origin + video_size_row * (top + 1)));
        }
    }
    else /* 不是EGA/VGA */
    {
        // 上移指定区域（软件实现）
        __asm__("cld\n\t"
                "rep\n\t"
                "movsl\n\t"
                "movl video_num_columns,%%ecx\n\t"
                "rep\n\t"
                "stosw" ::"a"(video_erase_char),
                "c"((bottom - top - 1) * video_num_columns >> 1),
                "D"(origin + video_size_row * top),
                "S"(origin + video_size_row * (top + 1)));
    }
}

// 向下滚动区域
static void scrdown(void)
{
    if (video_type == VIDEO_TYPE_EGAC || video_type == VIDEO_TYPE_EGAM)
    {
        // 下移指定区域（EGA/VGA硬件加速）
        __asm__("std\n\t"
                "rep\n\t"
                "movsl\n\t"
                "addl $2,%%edi\n\t" /* %edi已减4 */
                "movl video_num_columns,%%ecx\n\t"
                "rep\n\t"
                "stosw" ::"a"(video_erase_char),
                "c"((bottom - top - 1) * video_num_columns >> 1),
                "D"(origin + video_size_row * bottom - 4),
                "S"(origin + video_size_row * (bottom - 1) - 4));
    }
    else /* 不是EGA/VGA */
    {
        // 下移指定区域（软件实现）
        __asm__("std\n\t"
                "rep\n\t"
                "movsl\n\t"
                "addl $2,%%edi\n\t" /* %edi已减4 */
                "movl video_num_columns,%%ecx\n\t"
                "rep\n\t"
                "stosw" ::"a"(video_erase_char),
                "c"((bottom - top - 1) * video_num_columns >> 1),
                "D"(origin + video_size_row * bottom - 4),
                "S"(origin + video_size_row * (bottom - 1) - 4));
    }
}

// 换行（Line Feed）
static void lf(void)
{
    if (y + 1 < bottom) // 如果不在底部
    {
        y++;                   // 下移一行
        pos += video_size_row; // 更新位置
        return;
    }
    scrup(); // 否则向上滚动
}

// 反向换行（Reverse Index）
static void ri(void)
{
    if (y > top) // 如果不在顶部
    {
        y--;                   // 上移一行
        pos -= video_size_row; // 更新位置
        return;
    }
    scrdown(); // 否则向下滚动
}

// 回车（Carriage Return）
static void cr(void)
{
    pos -= x << 1; // 回到行首
    x = 0;         // 列重置为0
}

// 删除字符（Delete）
static void del(void)
{
    if (x) // 如果不在行首
    {
        pos -= 2;                                  // 前移一个位置
        x--;                                       // 列减1
        *(unsigned short *)pos = video_erase_char; // 用擦除字符覆盖
    }
}

// CSI J 命令 - 擦除显示
static void csi_J(int par)
{
    long count;
    long start;

    switch (par)
    {
    case 0:                           /* 从光标擦除到显示结束 */
        count = (scr_end - pos) >> 1; // 计算字符数
        start = pos;                  // 起始位置
        break;
    case 1:                          /* 从开始擦除到光标 */
        count = (pos - origin) >> 1; // 计算字符数
        start = origin;              // 起始位置
        break;
    case 2:                                          /* 擦除整个显示 */
        count = video_num_columns * video_num_lines; // 总字符数
        start = origin;                              // 起始位置
        break;
    default:
        return;
    }
    // 用擦除字符填充指定区域
    __asm__("cld\n\t"
            "rep\n\t"
            "stosw\n\t" ::"c"(count),
            "D"(start), "a"(video_erase_char));
}

// CSI K 命令 - 擦除行
static void csi_K(int par)
{
    long count;
    long start;

    switch (par)
    {
    case 0: /* 从光标擦除到行尾 */
        if (x >= video_num_columns)
            return;
        count = video_num_columns - x; // 计算字符数
        start = pos;                   // 起始位置
        break;
    case 1:                                                      /* 从行首擦除到光标 */
        start = pos - (x << 1);                                  // 起始位置
        count = (x < video_num_columns) ? x : video_num_columns; // 计算字符数
        break;
    case 2:                        /* 擦除整行 */
        start = pos - (x << 1);    // 起始位置
        count = video_num_columns; // 整行字符数
        break;
    default:
        return;
    }
    // 用擦除字符填充指定区域
    __asm__("cld\n\t"
            "rep\n\t"
            "stosw\n\t" ::"c"(count),
            "D"(start), "a"(video_erase_char));
}

// CSI m 命令 - 设置图形渲染
void csi_m(void)
{
    int i;

    for (i = 0; i <= npar; i++)
        switch (par[i])
        {
        case 0:          // 重置所有属性
            attr = 0x07; // 默认：灰色前景，黑色背景
            break;
        case 1:          // 粗体/高亮
            attr = 0x0f; // 白色前景，黑色背景
            break;
        case 4:          // 下划线（单色）
            attr = 0x0f; // 白色前景，黑色背景
            break;
        case 7:          // 反显
            attr = 0x70; // 黑色前景，灰色背景
            break;
        case 27:         // 取消反显
            attr = 0x07; // 灰色前景，黑色背景
            break;
        }
}

// 设置硬件光标位置
static inline void set_cursor(void)
{
    cli();                                                         // 关中断
    outb_p(14, video_port_reg);                                    // 选择光标位置高寄存器
    outb_p(0xff & ((pos - video_mem_start) >> 9), video_port_val); // 设置高字节
    outb_p(15, video_port_reg);                                    // 选择光标位置低寄存器
    outb_p(0xff & ((pos - video_mem_start) >> 1), video_port_val); // 设置低字节
    sti();                                                         // 开中断
}

// 响应终端查询
static void respond(struct tty_struct *tty)
{
    char *p = RESPONSE;

    cli();     // 关中断
    while (*p) // 写入响应字符串
    {
        PUTCH(*p, tty->read_q);
        p++;
    }
    sti();               // 开中断
    copy_to_cooked(tty); // 转换为加工模式
}

// 插入字符
static void insert_char(void)
{
    int i = x;
    unsigned short tmp, old = video_erase_char;
    unsigned short *p = (unsigned short *)pos;

    while (i++ < video_num_columns) // 右移字符
    {
        tmp = *p;
        *p = old;
        old = tmp;
        p++;
    }
}

// 插入行
static void insert_line(void)
{
    int oldtop, oldbottom;

    oldtop = top;
    oldbottom = bottom;
    top = y;
    bottom = video_num_lines;
    scrdown(); // 向下滚动
    top = oldtop;
    bottom = oldbottom;
}

// 删除字符
static void delete_char(void)
{
    int i;
    unsigned short *p = (unsigned short *)pos;

    if (x >= video_num_columns)
        return;
    i = x;
    while (++i < video_num_columns) // 左移字符
    {
        *p = *(p + 1);
        p++;
    }
    *p = video_erase_char; // 清除最后一个字符
}

// 删除行
static void delete_line(void)
{
    int oldtop, oldbottom;

    oldtop = top;
    oldbottom = bottom;
    top = y;
    bottom = video_num_lines;
    scrup(); // 向上滚动
    top = oldtop;
    bottom = oldbottom;
}

// CSI @ 命令 - 插入字符
static void csi_at(unsigned int nr)
{
    if (nr > video_num_columns)
        nr = video_num_columns;
    else if (!nr)
        nr = 1;
    while (nr--)
        insert_char(); // 插入指定数量的字符
}

// CSI L 命令 - 插入行
static void csi_L(unsigned int nr)
{
    if (nr > video_num_lines)
        nr = video_num_lines;
    else if (!nr)
        nr = 1;
    while (nr--)
        insert_line(); // 插入指定数量的行
}

// CSI P 命令 - 删除字符
static void csi_P(unsigned int nr)
{
    if (nr > video_num_columns)
        nr = video_num_columns;
    else if (!nr)
        nr = 1;
    while (nr--)
        delete_char(); // 删除指定数量的字符
}

// CSI M 命令 - 删除行
static void csi_M(unsigned int nr)
{
    if (nr > video_num_lines)
        nr = video_num_lines;
    else if (!nr)
        nr = 1;
    while (nr--)
        delete_line(); // 删除指定数量的行
}

static int saved_x = 0; // 保存的光标列
static int saved_y = 0; // 保存的光标行

// 保存光标位置
static void save_cur(void)
{
    saved_x = x;
    saved_y = y;
}

// 恢复光标位置
static void restore_cur(void)
{
    gotoxy(saved_x, saved_y);
}

// 控制台写函数
void con_write(struct tty_struct *tty)
{
    int nr;
    char c;

    nr = CHARS(tty->write_q); // 获取队列中的字符数
    while (nr--)
    {
        GETCH(tty->write_q, c); // 从队列获取字符
        switch (state)          // 根据状态处理字符
        {
        case 0:                    // 正常状态
            if (c > 31 && c < 127) // 可打印字符
            {
                if (x >= video_num_columns) // 如果超出列数
                {
                    x -= video_num_columns; // 回到行首
                    pos -= video_size_row;
                    lf(); // 换行
                }
                // 写入字符和属性
                __asm__("movb attr,%%ah\n\t"
                        "movw %%ax,%1\n\t" ::"a"(c),
                        "m"(*(short *)pos));
                pos += 2; // 移动位置
                x++;      // 列增加
            }
            else if (c == 27)                       // ESC字符
                state = 1;                          // 进入转义状态
            else if (c == 10 || c == 11 || c == 12) // 换行符
                lf();
            else if (c == 13) // 回车符
                cr();
            else if (c == ERASE_CHAR(tty)) // 擦除字符
                del();
            else if (c == 8) // 退格符
            {
                if (x)
                {
                    x--;
                    pos -= 2;
                }
            }
            else if (c == 9) // 制表符
            {
                c = 8 - (x & 7); // 计算空格数
                x += c;
                pos += c << 1;
                if (x > video_num_columns) // 如果超出列数
                {
                    x -= video_num_columns;
                    pos -= video_size_row;
                    lf(); // 换行
                }
                c = 9;
            }
            else if (c == 7) // 响铃符
                sysbeep();
            break;
        case 1: // 转义状态
            state = 0;
            if (c == '[') // CSI序列
                state = 2;
            else if (c == 'E') // 下一行行首
                gotoxy(0, y + 1);
            else if (c == 'M') // 反向换行
                ri();
            else if (c == 'D') // 换行
                lf();
            else if (c == 'Z') // 设备属性报告
                respond(tty);
            else if (x == '7') // 保存光标
                save_cur();
            else if (x == '8') // 恢复光标
                restore_cur();
            break;
        case 2: // CSI序列开始
            for (npar = 0; npar < NPAR; npar++)
                par[npar] = 0;
            npar = 0;
            state = 3;
            if ((ques = (c == '?'))) // 检查是否有问号
                break;
        case 3:                              // 参数收集
            if (c == ';' && npar < NPAR - 1) // 参数分隔符
            {
                npar++;
                break;
            }
            else if (c >= '0' && c <= '9') // 数字
            {
                par[npar] = 10 * par[npar] + c - '0'; // 构建参数
                break;
            }
            else
                state = 4; // 进入命令处理
        case 4:            // 命令处理
            state = 0;
            switch (c)
            {
            case 'G':
            case '`': // 水平定位
                if (par[0])
                    par[0]--;
                gotoxy(par[0], y);
                break;
            case 'A': // 光标上移
                if (!par[0])
                    par[0]++;
                gotoxy(x, y - par[0]);
                break;
            case 'B':
            case 'e': // 光标下移
                if (!par[0])
                    par[0]++;
                gotoxy(x, y + par[0]);
                break;
            case 'C':
            case 'a': // 光标右移
                if (!par[0])
                    par[0]++;
                gotoxy(x + par[0], y);
                break;
            case 'D': // 光标左移
                if (!par[0])
                    par[0]++;
                gotoxy(x - par[0], y);
                break;
            case 'E': // 光标下移n行到行首
                if (!par[0])
                    par[0]++;
                gotoxy(0, y + par[0]);
                break;
            case 'F': // 光标上移n行到行首
                if (!par[0])
                    par[0]++;
                gotoxy(0, y - par[0]);
                break;
            case 'd': // 垂直定位
                if (par[0])
                    par[0]--;
                gotoxy(x, par[0]);
                break;
            case 'H':
            case 'f': // 光标定位
                if (par[0])
                    par[0]--;
                if (par[1])
                    par[1]--;
                gotoxy(par[1], par[0]);
                break;
            case 'J': // 擦除显示
                csi_J(par[0]);
                break;
            case 'K': // 擦除行
                csi_K(par[0]);
                break;
            case 'L': // 插入行
                csi_L(par[0]);
                break;
            case 'M': // 删除行
                csi_M(par[0]);
                break;
            case 'P': // 删除字符
                csi_P(par[0]);
                break;
            case '@': // 插入字符
                csi_at(par[0]);
                break;
            case 'm': // 设置属性
                csi_m();
                break;
            case 'r': // 设置滚动区域
                if (par[0])
                    par[0]--;
                if (!par[1])
                    par[1] = video_num_lines;
                if (par[0] < par[1] &&
                    par[1] <= video_num_lines)
                {
                    top = par[0];
                    bottom = par[1];
                }
                break;
            case 's': // 保存光标
                save_cur();
                break;
            case 'u': // 恢复光标
                restore_cur();
                break;
            }
        }
    }
    set_cursor(); // 更新硬件光标
}

/*
 *  void con_init(void);
 *
 * 此例程初始化控制台中断，不执行其他操作。
 * 如果你想清屏，使用适当的转义序列调用tty_write。
 *
 * 读取由setup.s保存的信息以确定当前显示类型并相应设置所有内容。
 */
void con_init(void)
{
    register unsigned char a;
    char *display_desc = "????"; // 显示描述
    char *display_ptr;

    video_num_columns = ORIG_VIDEO_COLS;    // 设置列数
    video_size_row = video_num_columns * 2; // 每行字节数
    video_num_lines = ORIG_VIDEO_LINES;     // 设置行数
    video_page = ORIG_VIDEO_PAGE;           // 设置视频页
    video_erase_char = 0x0720;              // 设置擦除字符（空格+默认属性）

    if (ORIG_VIDEO_MODE == 7) /* 这是单色显示器吗？ */
    {
        video_mem_start = 0xb0000;              // 单显内存起始地址
        video_port_reg = 0x3b4;                 // 单显寄存器端口
        video_port_val = 0x3b5;                 // 单显数据端口
        if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10) // 如果是EGA单显
        {
            video_type = VIDEO_TYPE_EGAM;
            video_mem_end = 0xb8000;
            display_desc = "EGAm";
        }
        else // 否则是MDA单显
        {
            video_type = VIDEO_TYPE_MDA;
            video_mem_end = 0xb2000;
            display_desc = "*MDA";
        }
    }
    else /* 如果不是，就是彩色显示器 */
    {
        video_mem_start = 0xb8000;              // 彩显内存起始地址
        video_port_reg = 0x3d4;                 // 彩显寄存器端口
        video_port_val = 0x3d5;                 // 彩显数据端口
        if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10) // 如果是EGA彩显
        {
            video_type = VIDEO_TYPE_EGAC;
            video_mem_end = 0xbc000;
            display_desc = "EGAc";
        }
        else // 否则是CGA彩显
        {
            video_type = VIDEO_TYPE_CGA;
            video_mem_end = 0xba000;
            display_desc = "*CGA";
        }
    }

    /* 让用户知道我们正在使用哪种显示驱动程序 */

    display_ptr = ((char *)video_mem_start) + video_size_row - 8; // 显示在右下角
    while (*display_desc)
    {
        *display_ptr++ = *display_desc++; // 写入显示描述
        display_ptr++;
    }

    /* 初始化用于滚动的变量（主要是EGA/VGA）*/

    origin = video_mem_start;                                     // 起始地址
    scr_end = video_mem_start + video_num_lines * video_size_row; // 结束地址
    top = 0;                                                      // 滚动区域顶部
    bottom = video_num_lines;                                     // 滚动区域底部

    gotoxy(ORIG_X, ORIG_Y);                   // 设置初始光标位置
    set_trap_gate(0x21, &keyboard_interrupt); // 设置键盘中断
    outb_p(inb_p(0x21) & 0xfd, 0x21);         // 允许键盘中断
    a = inb_p(0x61);
    outb_p(a | 0x80, 0x61); // 禁用扬声器
    outb(a, 0x61);
}

// 停止蜂鸣
void sysbeepstop(void)
{
    /* 禁用计数器2 */
    outb(inb_p(0x61) & 0xFC, 0x61);
}

int beepcount = 0; // 蜂鸣计数

// 系统蜂鸣
static void sysbeep(void)
{
    /* 启用计数器2 */
    outb_p(inb_p(0x61) | 3, 0x61);
    /* 设置计数器2命令，2字节写入 */
    outb_p(0xB6, 0x43);
    /* 发送0x637得到750HZ */
    outb_p(0x37, 0x42);
    outb(0x06, 0x42);
    /* 1/8秒 */
    beepcount = HZ / 8; // 设置蜂鸣时间
}