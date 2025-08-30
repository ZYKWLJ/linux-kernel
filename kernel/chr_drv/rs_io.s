/*
 *  linux/kernel/rs_io.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *    rs_io.s
 *
 * 该模块实现RS232串口的I/O中断处理程序
 */

.text
.globl rs1_interrupt,rs2_interrupt  # 声明两个串口的中断处理函数为全局符号

size    = 1024                /* 缓冲区大小，必须是2的幂，且必须与tty_io.c中的值匹配 */

/* 以下是读写缓冲区结构中的偏移量定义 */
rs_addr = 0                   /* 串口端口地址在缓冲区结构中的偏移 */
head = 4                      /* 缓冲区头指针的偏移 */
tail = 8                      /* 缓冲区尾指针的偏移 */
proc_list = 12                /* 等待进程列表的偏移 */
buf = 16                      /* 实际缓冲区数据的偏移 */

startup    = 256              /* 当写队列中剩余字符数低于此值时，重启发送 */

/*
 * 这些是实际的中断处理程序。它们会判断中断来源，并采取相应的操作。
 */
.align 2                      /* 按4字节对齐 */
rs1_interrupt:                /* 第一个串口的中断处理入口 */
    pushl $table_list+8       /* 压入第一个串口的缓冲区结构地址（参数） */
    jmp rs_int                /* 跳转到通用中断处理程序 */
.align 2                      /* 按4字节对齐 */
rs2_interrupt:                /* 第二个串口的中断处理入口 */
    pushl $table_list+16      /* 压入第二个串口的缓冲区结构地址（参数） */
rs_int:                       /* 串口中断通用处理程序 */
    pushl %edx                /* 保存寄存器环境 */
    pushl %ecx
    pushl %ebx
    pushl %eax
    push %es
    push %ds                  /* 由于是中断，不能假设数据段正确，需要重新加载 */
    pushl $0x10               /* 加载内核数据段选择子（0x10）到ds */
    pop %ds
    pushl $0x10
    pop %es                   /* 加载内核数据段选择子到es */
    movl 24(%esp),%edx        /* 从栈中获取缓冲区结构地址（之前压入的table_list+8/16） */
    movl (%edx),%edx          /* 获取缓冲区结构中的实际数据（指向串口队列结构） */
    movl rs_addr(%edx),%edx   /* 获取串口的端口地址 */
    addl $2,%edx              /* 指向串口的中断识别寄存器（端口+2） */
rep_int:                      /* 循环处理所有未处理的中断 */
    xorl %eax,%eax
    inb %dx,%al               /* 读取中断识别寄存器的值 */
    testb $1,%al              /* 检查是否有未处理的中断（bit0为1表示无中断） */
    jne end                   /* 若无中断，跳转到结束处理 */
    cmpb $6,%al               /* 检查中断类型是否合法（应小于等于6） */
    ja end                    /* 若不合法，跳转到结束处理 */
    movl 24(%esp),%ecx        /* 再次获取缓冲区结构地址到ecx */
    pushl %edx                /* 保存当前edx（端口地址） */
    subl $2,%edx              /* 恢复到串口基地址（减去之前加的2） */
    call *jmp_table(,%eax,2)  /* 根据中断类型（eax）跳转到相应处理函数（注意是*2，因为bit0为0） */
    popl %edx                 /* 恢复edx */
    jmp rep_int               /* 继续处理其他中断 */
end:    movb $0x20,%al        /* 准备发送EOI（中断结束）信号 */
    outb %al,$0x20            /* 向主8259A中断控制器发送EOI */
    pop %ds                   /* 恢复寄存器环境 */
    pop %es
    popl %eax
    popl %ebx
    popl %ecx
    popl %edx
    addl $4,%esp              /* 跳过栈中的table_list入口参数 */
    iret                      /* 从中断返回 */

/* 中断处理函数跳转表，索引对应中断识别寄存器的值 */
jmp_table:
    .long modem_status,write_char,read_char,line_status

.align 2                      /* 按4字节对齐 */
modem_status:                 /* 调制解调器状态变化中断处理 */
    addl $6,%edx              /* 指向调制解调器状态寄存器（端口+6） */
    inb %dx,%al               /* 读取该寄存器以清除中断 */
    ret                       /* 返回继续处理其他中断 */

.align 2                      /* 按4字节对齐 */
line_status:                  /* 线路状态变化中断处理 */
    addl $5,%edx              /* 指向线路状态寄存器（端口+5） */
    inb %dx,%al               /* 读取该寄存器以清除中断 */
    ret                       /* 返回继续处理其他中断 */

.align 2                      /* 按4字节对齐 */
read_char:                    /* 接收字符中断处理（数据就绪） */
    inb %dx,%al               /* 从数据接收寄存器（端口+0）读取字符 */
    movl %ecx,%edx            /* 保存缓冲区结构地址到edx */
    subl $table_list,%edx     /* 计算相对偏移 */
    shrl $3,%edx              /* 右移3位（除以8），用于确定终端号 */
    movl (%ecx),%ecx          /* 获取读队列结构 */
    movl head(%ecx),%ebx      /* 获取读队列的头指针 */
    movb %al,buf(%ecx,%ebx)   /* 将接收的字符存入缓冲区 */
    incl %ebx                 /* 头指针自增 */
    andl $size-1,%ebx         /* 对缓冲区大小取模（循环缓冲区） */
    cmpl tail(%ecx),%ebx      /* 检查缓冲区是否已满（头指针是否等于尾指针） */
    je 1f                     /* 若满，则不更新头指针（丢弃当前字符） */
    movl %ebx,head(%ecx)      /* 若未满，更新头指针 */
1:    pushl %edx              /* 保存终端号参数 */
    call do_tty_interrupt     /* 调用终端中断处理函数，通知有数据到达 */
    addl $4,%esp              /* 清理栈中的参数 */
    ret                       /* 返回继续处理其他中断 */

.align 2                      /* 按4字节对齐 */
write_char:                   /* 发送字符中断处理（发送缓冲区为空） */
    movl 4(%ecx),%ecx         /* 获取写队列结构（从缓冲区结构的+4偏移） */
    movl head(%ecx),%ebx      /* 获取写队列的头指针 */
    subl tail(%ecx),%ebx      /* 计算队列中字符数（头 - 尾） */
    andl $size-1,%ebx         /* 对缓冲区大小取模，得到实际字符数 */
    je write_buffer_empty     /* 若队列为空，跳转到处理空缓冲区 */
    cmpl $startup,%ebx        /* 检查字符数是否大于启动阈值 */
    ja 1f                     /* 若大于，则不唤醒进程，直接继续发送 */
    movl proc_list(%ecx),%ebx /* 获取等待发送的进程列表 */
    testl %ebx,%ebx           /* 检查是否有等待的进程 */
    je 1f                     /* 若无，继续发送 */
    movl $0,(%ebx)            /* 若有，唤醒进程（设置其等待状态为0） */
1:    movl tail(%ecx),%ebx    /* 获取写队列的尾指针 */
    movb buf(%ecx,%ebx),%al   /* 从缓冲区取出要发送的字符 */
    outb %al,%dx              /* 写入数据发送寄存器（端口+0） */
    incl %ebx                 /* 尾指针自增 */
    andl $size-1,%ebx         /* 对缓冲区大小取模 */
    movl %ebx,tail(%ecx)      /* 更新尾指针 */
    cmpl head(%ecx),%ebx      /* 检查发送是否完成（尾指针是否等于头指针） */
    je write_buffer_empty     /* 若完成，跳转到处理空缓冲区 */
    ret                       /* 未完成，返回继续处理其他中断 */
.align 2                      /* 按4字节对齐 */
write_buffer_empty:           /* 写缓冲区为空时的处理 */
    movl proc_list(%ecx),%ebx /* 获取等待发送的进程列表 */
    testl %ebx,%ebx           /* 检查是否有等待的进程 */
    je 1f                     /* 若无，继续 */
    movl $0,(%ebx)            /* 若有，唤醒进程 */
1:    incl %edx               /* 指向中断允许寄存器（端口+1） */
    inb %dx,%al               /* 读取当前中断允许寄存器值 */
    jmp 1f                    /* 延迟，确保硬件准备好 */
1:    jmp 1f
1:    andb $0xd,%al           /* 禁用发送中断（清除bit1），保留其他位（0x0d = 1101） */
    outb %al,%dx              /* 写入中断允许寄存器，生效设置 */
    ret                       /* 返回 */