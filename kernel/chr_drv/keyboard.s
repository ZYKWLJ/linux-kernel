/*
 *  linux/kernel/keyboard.S
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *	感谢 Alfred Leung 提供美式键盘补丁
 *		Wolfgang Thiel 提供德式键盘补丁  
 *		Marc Corsini 提供法式键盘
 */

#include <linux/config.h>  // 引入 Linux 配置头文件

.text                   // 代码段开始
.globl keyboard_interrupt  // 声明全局符号 keyboard_interrupt

/*
 * 这些是用于键盘读取函数的常量
 */
size    = 1024        /* 必须是2的幂！且必须与 tty_io.c 中的相同！！！ */
head = 4              // 队列头指针偏移量
tail = 8              // 队列尾指针偏移量  
proc_list = 12        // 等待进程列表偏移量
buf = 16              // 缓冲区偏移量

mode:   .byte 0       /* 大小写、alt、ctrl 和 shift 模式状态 */
leds:   .byte 2       /* 数字锁定、大写锁定、滚动锁定模式（默认数字锁定开启） */
e0:     .byte 0       /* E0 扩展前缀标志 */

/*
 *  con_int 是实际的中断处理程序，它读取
 *  键盘扫描码并将其转换为相应的
 *  ASCII 字符。
 */
keyboard_interrupt:
    pushl %eax        // 保存寄存器
    pushl %ebx
    pushl %ecx
    pushl %edx
    push %ds          // 保存数据段寄存器
    push %es          // 保存附加段寄存器
    movl $0x10,%eax   // 设置内核数据段选择子 (0x10)
    mov %ax,%ds       // 设置数据段
    mov %ax,%es       // 设置附加段
    xor %al,%al       /* %eax 清零，准备读取扫描码 */
    inb $0x60,%al     // 从键盘数据端口 (0x60) 读取扫描码
    
    cmpb $0xe0,%al    // 检查是否是 E0 前缀
    je set_e0         // 如果是，跳转到 set_e0
    cmpb $0xe1,%al    // 检查是否是 E1 前缀  
    je set_e1         // 如果是，跳转到 set_e1
    
    call *key_table(,%eax,4)  // 调用键处理函数（根据扫描码在key_table中查找）
    
    movb $0,e0        // 清除 E0 标志
e0_e1: 
    inb $0x61,%al     // 读取键盘控制器状态
    jmp 1f            // 短延时
1:  jmp 1f            // 短延时
1:  orb $0x80,%al     // 设置"已响应"标志位
    jmp 1f            // 短延时
1:  jmp 1f            // 短延时
1:  outb %al,$0x61    // 写回键盘控制器，确认中断处理
    
    jmp 1f            // 短延时
1:  jmp 1f            // 短延时
1:  andb $0x7F,%al    // 清除"已响应"标志位
    outb %al,$0x61    // 写回键盘控制器
    
    movb $0x20,%al    // 发送 EOI (End Of Interrupt) 命令
    outb %al,$0x20    // 到中断控制器 (8259A)
    
    pushl $0          // 压入参数 (tty号，0表示控制台)
    call do_tty_interrupt  // 调用 tty 中断处理函数
    addl $4,%esp      // 清理堆栈
    
    pop %es           // 恢复寄存器
    pop %ds
    popl %edx
    popl %ecx
    popl %ebx
    popl %eax
    iret              // 中断返回
    
set_e0:
    movb $1,e0        // 设置 E0 标志为 1
    jmp e0_e1         // 跳回主流程
    
set_e1:
    movb $2,e0        // 设置 E0 标志为 2 (E1前缀)
    jmp e0_e1         // 跳回主流程

/*
 * 此例程用最多8个字节填充缓冲区，这些字节取自
 * %ebx:%eax。（%edx 是高32位）。字节按
 * %al,%ah,%eal,%eah,%bl,%bh ... 的顺序写入，直到 %eax 为零。
 */
put_queue:
    pushl %ecx        // 保存寄存器
    pushl %edx
    movl table_list,%edx      // 获取控制台的读队列地址
    movl head(%edx),%ecx      // 获取队列头指针
    
1:  movb %al,buf(%edx,%ecx)   // 将字符存入缓冲区
    incl %ecx                 // 头指针递增
    andl $size-1,%ecx         // 头指针取模（循环缓冲区）
    
    cmpl tail(%edx),%ecx      // 检查缓冲区是否已满
    je 3f                     // 如果已满，跳转到3（丢弃所有内容）
    
    shrdl $8,%ebx,%eax        // 将下一个字节移到%al
    je 2f                     // 如果%eax为零，跳转到2
    shrl $8,%ebx              // 准备下一个字节
    jmp 1b                    // 继续循环
    
2:  movl %ecx,head(%edx)      // 更新队列头指针
    movl proc_list(%edx),%ecx // 获取等待进程列表
    testl %ecx,%ecx           // 检查是否有等待进程
    je 3f                     // 如果没有，跳转到3
    movl $0,(%ecx)            // 唤醒进程（设置进程状态为0）
    
3:  popl %edx                 // 恢复寄存器
    popl %ecx
    ret                       // 返回

// Ctrl 键按下处理
ctrl:   
    movb $0x04,%al    // Ctrl 模式位
    jmp 1f
// Alt 键按下处理    
alt:    
    movb $0x10,%al    // Alt 模式位
1:  cmpb $0,e0        // 检查是否有 E0 前缀
    je 2f             // 如果没有，跳转到2
    addb %al,%al      // 对于扩展键，使用不同的位（右Ctrl/Alt）
2:  orb %al,mode      // 设置模式位
    ret

// Ctrl 键释放处理    
unctrl: 
    movb $0x04,%al    // Ctrl 模式位
    jmp 1f
// Alt 键释放处理    
unalt:  
    movb $0x10,%al    // Alt 模式位
1:  cmpb $0,e0        // 检查是否有 E0 前缀
    je 2f             // 如果没有，跳转到2
    addb %al,%al      // 对于扩展键，使用不同的位
2:  notb %al          // 取反，创建掩码
    andb %al,mode     // 清除模式位
    ret

// 左Shift键按下处理
lshift:
    orb $0x01,mode    // 设置左Shift位
    ret
// 左Shift键释放处理    
unlshift:
    andb $0xfe,mode   // 清除左Shift位
    ret
// 右Shift键按下处理    
rshift:
    orb $0x02,mode    // 设置右Shift位
    ret
// 右Shift键释放处理    
unrshift:
    andb $0xfd,mode   // 清除右Shift位
    ret

// CapsLock键处理
caps:   
    testb $0x80,mode  // 检查是否已处理过CapsLock
    jne 1f            // 如果已处理，跳转到1（防止重复处理）
    xorb $4,leds      // 切换CapsLock LED状态
    xorb $0x40,mode   // 切换CapsLock模式位
    orb $0x80,mode    // 设置"已处理"标志
set_leds:
    call kb_wait      // 等待键盘控制器就绪
    movb $0xed,%al    /* 设置LED命令 */
    outb %al,$0x60    // 发送命令到键盘
    call kb_wait      // 等待键盘控制器就绪
    movb leds,%al     // 获取LED状态
    outb %al,$0x60    // 发送LED状态到键盘
    ret
// CapsLock键释放处理    
uncaps: 
    andb $0x7f,mode   // 清除"已处理"标志
    ret

// ScrollLock键处理
scroll:
    xorb $1,leds      // 切换ScrollLock LED
    jmp set_leds      // 跳转到设置LED

// NumLock键处理    
num:    
    xorb $2,leds      // 切换NumLock LED
    jmp set_leds      // 跳转到设置LED

/*
 *  光标键/数字小键盘光标键在这里处理。
 *  检查数字小键盘等。
 */
cursor:
    subb $0x47,%al    // 减去Home键的扫描码基数
    jb 1f             // 如果小于0，跳转到1（返回）
    cmpb $12,%al      // 检查是否超出范围
    ja 1f             // 如果大于12，跳转到1（返回）
    
    jne cur2          /* 检查Ctrl-Alt-Del */
    testb $0x0c,mode  // 检查是否同时按下Ctrl和Alt
    je cur2           // 如果不是，跳转到cur2
    testb $0x30,mode  // 检查是否同时按下左Alt和右Alt
    jne reboot        // 如果是，跳转到重启
    
cur2:   cmpb $0x01,e0        /* E0 强制光标移动 */
    je cur            // 如果是E0前缀，跳转到光标处理
    testb $0x02,leds  /* 非数字锁定状态强制光标 */
    je cur            // 如果NumLock关闭，跳转到光标处理
    testb $0x03,mode  /* Shift 键强制光标 */
    jne cur           // 如果按下Shift，跳转到光标处理
    
    xorl %ebx,%ebx    // 清零%ebx（高32位）
    movb num_table(%eax),%al  // 从数字表获取数字字符
    jmp put_queue     // 将字符放入队列
    
1:  ret               // 返回

// 光标键处理
cur:    
    movb cur_table(%eax),%al  // 从光标表获取光标字符
    cmpb $'9,%al      // 检查是否是数字字符
    ja ok_cur         // 如果不是，跳转到ok_cur
    movb $'~,%ah      // 设置前缀字符'~（用于特殊光标序列）
    
ok_cur: 
    shll $16,%eax     // 将字符移到高位
    movw $0x5b1b,%ax  // 设置转义序列前缀 ESC-[
    xorl %ebx,%ebx    // 清零%ebx
    jmp put_queue     // 将转义序列放入队列

// 数字小键盘映射表
#if defined(KBD_FR)   // 法式键盘
num_table:
    .ascii "789 456 1230."
#else                 // 其他键盘
num_table:
    .ascii "789 456 1230,"
#endif

// 光标键映射表
cur_table:
    .ascii "HA5 DGC YB623"  // Home, Up, PgUp, -, Left, 5, Right, +, End, Down, PgDn, Ins, Del

/*
 * 此例程处理功能键
 */
func:
    pushl %eax        // 保存寄存器
    pushl %ecx
    pushl %edx
    call show_stat    // 显示系统状态（进程信息）
    popl %edx         // 恢复寄存器
    popl %ecx
    popl %eax
    
    subb $0x3B,%al    // 减去F1键的扫描码基数
    jb end_func       // 如果小于0，跳转到结束（无效功能键）
    cmpb $9,%al       // 检查是否是F1-F10
    jbe ok_func       // 如果是，跳转到ok_func
    
    subb $18,%al      // 调整F11-F12的索引
    cmpb $10,%al      // 检查是否是F11
    jb end_func       // 如果小于10，跳转到结束（无效）
    cmpb $11,%al      // 检查是否是F12
    ja end_func       // 如果大于11，跳转到结束（无效）
    
ok_func:
    cmpl $4,%ecx      /* 检查队列是否有足够空间（至少4字节） */
    jl end_func       // 如果空间不足，跳转到结束
    
    movl func_table(,%eax,4),%eax  // 从功能键表获取转义序列
    xorl %ebx,%ebx    // 清零%ebx
    jmp put_queue     // 将转义序列放入队列
    
end_func:
    ret               // 返回

/*
 * 功能键发送 F1: 'esc [ [ A' F2: 'esc [ [ B' 等。
 */
func_table:
    .long 0x415b5b1b,0x425b5b1b,0x435b5b1b,0x445b5b1b  // F1-F4: ESC[[A, ESC[[B, ...
    .long 0x455b5b1b,0x465b5b1b,0x475b5b1b,0x485b5b1b  // F5-F8
    .long 0x495b5b1b,0x4a5b5b1b,0x4b5b5b1b,0x4c5b5b1b  // F9-F12

// 键盘映射表 - 根据不同语言键盘配置
#if defined(KBD_FINNISH)  // 芬兰语键盘
key_map:
    .byte 0,27        // 扫描码 0x00, 0x01
    .ascii "1234567890+'"  // 扫描码 0x02-0x0D
    .byte 127,9       // 扫描码 0x0E, 0x0F
    .ascii "qwertyuiop}"   // 扫描码 0x10-0x1B
    .byte 0,13,0      // 扫描码 0x1C, 0x1D, 0x1E
    .ascii "asdfghjkl|{"   // 扫描码 0x1F-0x29
    .byte 0,0         // 扫描码 0x2A, 0x2B
    .ascii "'zxcvbnm,.-"   // 扫描码 0x2C-0x35
    .byte 0,'*,0,32   /* 扫描码 0x36-0x39 */
    .fill 16,1,0      /* 扫描码 0x3A-0x49 */
    .byte '-,0,0,0,'+ /* 扫描码 0x4A-0x4E */
    .byte 0,0,0,0,0,0,0 /* 扫描码 0x4F-0x55 */
    .byte '<          // 扫描码 0x56
    .fill 10,1,0      // 扫描码 0x57-0x60

// Shift键映射表
shift_map:
    .byte 0,27
    .ascii "!\"#$%&/()=?`"
    .byte 127,9
    .ascii "QWERTYUIOP]^"
    .byte 13,0
    .ascii "ASDFGHJKL\\["
    .byte 0,0
    .ascii "*ZXCVBNM;:_"
    .byte 0,'*,0,32   /* 36-39 */
    .fill 16,1,0      /* 3A-49 */
    .byte '-,0,0,0,'+ /* 4A-4E */
    .byte 0,0,0,0,0,0,0 /* 4F-55 */
    .byte '>
    .fill 10,1,0

// Alt键映射表
alt_map:
    .byte 0,0
    .ascii "\0@\0$\0\0{[]}\\\0"
    .byte 0,0
    .byte 0,0,0,0,0,0,0,0,0,0,0
    .byte '~,13,0
    .byte 0,0,0,0,0,0,0,0,0,0,0
    .byte 0,0
    .byte 0,0,0,0,0,0,0,0,0,0,0
    .byte 0,0,0,0     /* 36-39 */
    .fill 16,1,0      /* 3A-49 */
    .byte 0,0,0,0,0   /* 4A-4E */
    .byte 0,0,0,0,0,0,0 /* 4F-55 */
    .byte '|
    .fill 10,1,0

#elif defined(KBD_US)  // 美式键盘（默认）

key_map:
    .byte 0,27
    .ascii "1234567890-="  // 扫描码 0x02-0x0D
    .byte 127,9
    .ascii "qwertyuiop[]"  // 扫描码 0x10-0x1B
    .byte 13,0
    .ascii "asdfghjkl;'"   // 扫描码 0x1F-0x28
    .byte '`,0
    .ascii "\\zxcvbnm,./"  // 扫描码 0x2B-0x35
    .byte 0,'*,0,32   /* 扫描码 0x36-0x39 */
    .fill 16,1,0      /* 扫描码 0x3A-0x49 */
    .byte '-,0,0,0,'+ /* 扫描码 0x4A-0x4E */
    .byte 0,0,0,0,0,0,0 /* 扫描码 0x4F-0x55 */
    .byte '<          // 扫描码 0x56
    .fill 10,1,0      // 扫描码 0x57-0x60

shift_map:
    .byte 0,27
    .ascii "!@#$%^&*()_+"
    .byte 127,9
    .ascii "QWERTYUIOP{}"
    .byte 13,0
    .ascii "ASDFGHJKL:\""
    .byte '~,0
    .ascii "|ZXCVBNM<>?"
    .byte 0,'*,0,32   /* 36-39 */
    .fill 16,1,0      /* 3A-49 */
    .byte '-,0,0,0,'+ /* 4A-4E */
    .byte 0,0,0,0,0,0,0 /* 4F-55 */
    .byte '>
    .fill 10,1,0

alt_map:
    .byte 0,0
    .ascii "\0@\0$\0\0{[]}\\\0"
    .byte 0,0
    .byte 0,0,0,0,0,0,0,0,0,0,0
    .byte '~,13,0
    .byte 0,0,0,0,0,0,0,0,0,0,0
    .byte 0,0
    .byte 0,0,0,0,0,0,0,0,0,0,0
    .byte 0,0,0,0     /* 36-39 */
    .fill 16,1,0      /* 3A-49 */
    .byte 0,0,0,0,0   /* 4A-4E */
    .byte 0,0,0,0,0,0,0 /* 4F-55 */
    .byte '|
    .fill 10,1,0

#elif defined(KBD_GR)  // 德语键盘

key_map:
    .byte 0,27
    .ascii "1234567890\\'"
    .byte 127,9
    .ascii "qwertzuiop@+"
    .byte 13,0
    .ascii "asdfghjkl[]^"
    .byte 0,'#
    .ascii "yxcvbnm,.-"
    .byte 0,'*,0,32   /* 36-39 */
    .fill 16,1,0      /* 3A-49 */
    .byte '-,0,0,0,'+ /* 4A-4E */
    .byte 0,0,0,0,0,0,0 /* 4F-55 */
    .byte '<
    .fill 10,1,0

shift_map:
    .byte 0,27
    .ascii "!\"#$%&/()=?`"
    .byte 127,9
    .ascii "QWERTZUIOP\\*"
    .byte 13,0
    .ascii "ASDFGHJKL{}~"
    .byte 0,''
    .ascii "YXCVBNM;:_"
    .byte 0,'*,0,32   /* 36-39 */
    .fill 16,1,0      /* 3A-49 */
    .byte '-,0,0,0,'+ /* 4A-4E */
    .byte 0,0,0,0,0,0,0 /* 4F-55 */
    .byte '>
    .fill 10,1,0

alt_map:
    .byte 0,0
    .ascii "\0@\0$\0\0{[]}\\\0"
    .byte 0,0
    .byte '@,0,0,0,0,0,0,0,0,0,0
    .byte '~,13,0
    .byte 0,0,0,0,0,0,0,0,0,0,0
    .byte 0,0
    .byte 0,0,0,0,0,0,0,0,0,0,0
    .byte 0,0,0,0     /* 36-39 */
    .fill 16,1,0      /* 3A-49 */
    .byte 0,0,0,0,0   /* 4A-4E */
    .byte 0,0,0,0,0,0,0 /* 4F-55 */
    .byte '|
    .fill 10,1,0

#elif defined(KBD_FR)  // 法语键盘

key_map:
    .byte 0,27
    .ascii "&{\"'(-}_/@)="
    .byte 127,9
    .ascii "azertyuiop^$"
    .byte 13,0
    .ascii "qsdfghjklm|"
    .byte '`,0,42     /* 左上角键，不知道，[*|mu] */
    .ascii "wxcvbn,;:!"
    .byte 0,'*,0,32   /* 36-39 */
    .fill 16,1,0      /* 3A-49 */
    .byte '-,0,0,0,'+ /* 4A-4E */
    .byte 0,0,0,0,0,0,0 /* 4F-55 */
    .byte '<
    .fill 10,1,0

shift_map:
    .byte 0,27
    .ascii "1234567890]+"
    .byte 127,9
    .ascii "AZERTYUIOP<>"
    .byte 13,0
    .ascii "QSDFGHJKLM%"
    .byte '~,0,'#
    .ascii "WXCVBN?./\\"
    .byte 0,'*,0,32   /* 36-39 */
    .fill 16,1,0      /* 3A-49 */
    .byte '-,0,0,0,'+ /* 4A-4E */
    .byte 0,0,0,0,0,0,0 /* 4F-55 */
    .byte '>
    .fill 10,1,0

alt_map:
    .byte 0,0
    .ascii "\0~#{[|`\\^@]}"
    .byte 0,0
    .byte '@,0,0,0,0,0,0,0,0,0,0
    .byte '~,13,0
    .byte 0,0,0,0,0,0,0,0,0,0,0
    .byte 0,0
    .byte 0,0,0,0,0,0,0,0,0,0,0
    .byte 0,0,0,0     /* 36-39 */
    .fill 16,1,0      /* 3A-49 */
    .byte 0,0,0,0,0   /* 4A-4E */
    .byte 0,0,0,0,0,0,0 /* 4F-55 */
    .byte '|
    .fill 10,1,0

#else
#error "KBD-type not defined"  // 如果没有定义键盘类型，报错
#endif

/*
 * do_self 处理"普通"键，即不改变含义
 * 且只有一个字符返回的键。
 */
do_self:
    lea alt_map,%ebx  // 加载Alt映射表地址
    testb $0x20,mode  // 检查AltGr模式（右Alt）
    jne 1f            // 如果按下AltGr，使用Alt映射表
    
    lea shift_map,%ebx  // 加载Shift映射表地址
    testb $0x03,mode  // 检查Shift模式
    jne 1f            // 如果按下Shift，使用Shift映射表
    
    lea key_map,%ebx  // 否则使用普通映射表
    
1:  movb (%ebx,%eax),%al  // 根据扫描码从映射表获取字符
    orb %al,%al       // 检查是否有效字符
    je none           // 如果为零，跳转到none（返回）
    
    testb $0x4c,mode  // 检查Ctrl或CapsLock模式
    je 2f             // 如果没有，跳转到2
    cmpb $'a,%al      // 检查是否小写字母
    jb 2f             // 如果小于'a'，跳转到2
    cmpb $'},%al      // 检查是否超出字母范围
    ja 2f             // 如果大于'}'，跳转到2
    subb $32,%al      // 转换为大写
    
2:  testb $0x0c,mode  // 检查Ctrl模式
    je 3f             // 如果没有，跳转到3
    cmpb $64,%al      // 检查是否可转换为控制字符
    jb 3f             // 如果小于64，跳转到3
    cmpb $64+32,%al   // 检查是否超出范围
    jae 3f            // 如果大于等于96，跳转到3
    subb $64,%al      // 转换为控制字符（0-31）
    
3:  testb $0x10,mode  // 检查左Alt模式
    je 4f             // 如果没有，跳转到4
    orb $0x80,%al     // 设置高位（生成扩展ASCII）
    
4:  andl $0xff,%eax   // 确保%eax只有字符值
    xorl %ebx,%ebx    // 清零%ebx
    call put_queue    // 将字符放入队列
    
none:
    ret               // 返回

/*
 * 减号有它自己的例程，因为扫描码前的'E0h'
 * 意味着按下了数字小键盘的斜杠键。
 */
minus:  
    cmpb $1,e0        // 检查是否有E0前缀
    jne do_self       // 如果没有，按普通减号处理
    movl $'/,%eax     // 设置为斜杠字符
    xorl %ebx,%ebx    // 清零%ebx
    jmp put_queue     // 将字符放入队列

/*
 * 此表决定当获取到扫描码时要调用哪个例程。
 * 大多数例程只调用 do_self 或 none，取决于
 * 它们是按下还是释放。
 */
key_table:
    .long none,do_self,do_self,do_self   /* 00-03 s0 esc 1 2 */
    .long do_self,do_self,do_self,do_self   /* 04-07 3 4 5 6 */
    .long do_self,do_self,do_self,do_self   /* 08-0B 7 8 9 0 */
    .long do_self,do_self,do_self,do_self   /* 0C-0F + ' bs tab */
    .long do_self,do_self,do_self,do_self   /* 10-13 q w e r */
    .long do_self,do_self,do_self,do_self   /* 14-17 t y u i */
    .long do_self,do_self,do_self,do_self   /* 18-1B o p } ^ */
    .long do_self,ctrl,do_self,do_self   /* 1C-1F enter ctrl a s */
    .long do_self,do_self,do_self,do_self   /* 20-23 d f g h */
    .long do_self,do_self,do_self,do_self   /* 24-27 j k l | */
    .long do_self,do_self,lshift,do_self   /* 28-2B { para lshift , */
    .long do_self,do_self,do_self,do_self   /* 2C-2F z x c v */
    .long do_self,do_self,do_self,do_self   /* 30-33 b n m , */
    .long do_self,minus,rshift,do_self   /* 34-37 . - rshift * */
    .long alt,do_self,caps,func      /* 38-3B alt sp caps f1 */
    .long func,func,func,func        /* 3C-3F f2 f3 f4 f5 */
    .long func,func,func,func        /* 40-43 f6 f7 f8 f9 */
    .long func,num,scroll,cursor     /* 44-47 f10 num scr home */
    .long cursor,cursor,do_self,cursor   /* 48-4B up pgup - left */
    .long cursor,cursor,do_self,cursor   /* 4C-4F n5 right + end */
    .long cursor,cursor,cursor,cursor   /* 50-53 dn pgdn ins del */
    .long none,none,do_self,func     /* 54-57 sysreq ? < f11 */
    .long func,none,none,none        /* 58-5B f12 ? ? ? */
    .long none,none,none,none        /* 5C-5F ? ? ? ? */
    .long none,none,none,none        /* 60-63 ? ? ? ? */
    .long none,none,none,none        /* 64-67 ? ? ? ? */
    .long none,none,none,none        /* 68-6B ? ? ? ? */
    .long none,none,none,none        /* 6C-6F ? ? ? ? */
    .long none,none,none,none        /* 70-73 ? ? ? ? */
    .long none,none,none,none        /* 74-77 ? ? ? ? */
    .long none,none,none,none        /* 78-7B ? ? ? ? */
    .long none,none,none,none        /* 7C-7F ? ? ? ? */
    .long none,none,none,none        /* 80-83 ? br br br */
    .long none,none,none,none        /* 84-87 br br br br */
    .long none,none,none,none        /* 88-8B br br br br */
    .long none,none,none,none        /* 8C-8F br br br br */
    .long none,none,none,none        /* 90-93 br br br br */
    .long none,none,none,none        /* 94-97 br br br br */
    .long none,none,none,none        /* 98-9B br br br br */
    .long none,unctrl,none,none      /* 9C-9F br unctrl br br */
    .long none,none,none,none        /* A0-A3 br br br br */
    .long none,none,none,none        /* A4-A7 br br br br */
    .long none,none,unlshift,none    /* A8-AB br br unlshift br */
    .long none,none,none,none        /* AC-AF br br br br */
    .long none,none,none,none        /* B0-B3 br br br br */
    .long none,none,unrshift,none    /* B4-B7 br br unrshift br */
    .long unalt,none,uncaps,none     /* B8-BB unalt br uncaps br */
    .long none,none,none,none        /* BC-BF br br br br */
    .long none,none,none,none        /* C0-C3 br br br br */
    .long none,none,none,none        /* C4-C7 br br br br */
    .long none,none,none,none        /* C8-CB br br br br */
    .long none,none,none,none        /* CC-CF br br br br */
    .long none,none,none,none        /* D0-D3 br br br br */
    .long none,none,none,none        /* D4-D7 br br br br */
    .long none,none,none,none        /* D8-DB br ? ? ? */
    .long none,none,none,none        /* DC-DF ? ? ? ? */
    .long none,none,none,none        /* E0-E3 e0 e1 ? ? */
    .long none,none,none,none        /* E4-E7 ? ? ? ? */
    .long none,none,none,none        /* E8-EB ? ? ? ? */
    .long none,none,none,none        /* EC-EF ? ? ? ? */
    .long none,none,none,none        /* F0-F3 ? ? ? ? */
    .long none,none,none,none        /* F4-F7 ? ? ? ? */
    .long none,none,none,none        /* F8-FB ? ? ? ? */
    .long none,none,none,none        /* FC-FF ? ? ? ? */

/*
 * kb_wait 等待键盘控制器缓冲区为空。
 * 没有超时 - 如果缓冲区不为空，我们会挂起。
 */
kb_wait:
    pushl %eax        // 保存%eax
1:  inb $0x64,%al     // 读取键盘控制器状态
    testb $0x02,%al   // 检查输入缓冲区是否满
    jne 1b            // 如果满，继续等待
    popl %eax         // 恢复%eax
    ret               // 返回

/*
 * 此例程通过请求键盘控制器
 * 将复位线拉低来重新启动机器。
 */
reboot:
    call kb_wait      // 等待键盘控制器就绪
    movw $0x1234,0x472  /* 不进行内存检查 */
    movb $0xfc,%al    /* 将复位和A20线拉低 */
    outb %al,$0x64    // 发送命令到键盘控制器
die:    
    jmp die           // 死循环，等待复位