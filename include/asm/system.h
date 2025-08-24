/*
 * 切换到用户模式
 * 该宏通过一系列汇编指令完成从内核模式到用户模式的切换
 */
#define move_to_user_mode()            \
    __asm__("movl %%esp,%%eax\n\t"     /* 将当前栈指针ESP保存到EAX寄存器 */ \
            "pushl $0x17\n\t"          /* 压入用户数据段选择子(0x17 = 0x10 + 3, RPL=3) */ \
            "pushl %%eax\n\t"          /* 压入之前保存的ESP值(用户模式栈指针) */ \
            "pushfl\n\t"               /* 压入EFLAGS寄存器值 */ \
            "pushl $0x0f\n\t"          /* 压入用户代码段选择子(0x0f = 0x08 + 3, RPL=3) */ \
            "pushl $1f\n\t"            /* 压入中断返回后要执行的指令地址(标签1处) */ \
            "iret\n"                   /* 执行中断返回, 这会将上面压入的值弹出到对应的寄存器: */ \
                                       /* CS = 0x0f, EIP = 1f, EFLAGS恢复, ESP恢复, SS = 0x17 */ \
            "1:\tmovl $0x17,%%eax\n\t" /* 切换到用户数据段 */ \
            "movw %%ax,%%ds\n\t"       /* 设置数据段寄存器DS */ \
            "movw %%ax,%%es\n\t"       /* 设置附加段寄存器ES */ \
            "movw %%ax,%%fs\n\t"       /* 设置附加段寄存器FS */ \
            "movw %%ax,%%gs" ::: "ax") /* 设置附加段寄存器GS, 并声明AX寄存器被修改 */

/* 启用中断 */
#define sti() __asm__("sti" ::)  /* 执行sti汇编指令, 置位EFLAGS中的IF标志 */

/* 禁用中断 */
#define cli() __asm__("cli" ::)  /* 执行cli汇编指令, 清除EFLAGS中的IF标志 */

/* 空操作 */
#define nop() __asm__("nop" ::)  /* 执行nop汇编指令, 无实际操作, 用于延迟或对齐 */

/* 中断返回 */
#define iret() __asm__("iret" ::) /* 执行iret汇编指令, 从中断处理程序返回 */

/*
 * 设置门描述符
 * gate_addr: 门描述符地址
 * type: 门类型(中断门/陷阱门等)
 * dpl: 描述符特权级
 * addr: 门指向的处理程序地址
 */
#define _set_gate(gate_addr, type, dpl, addr)                   \
    __asm__("movw %%dx,%%ax\n\t"                                /* 将DX(低16位)移动到AX的低16位 */ \
            "movw %0,%%dx\n\t"                                  /* 将类型和特权级信息移动到DX */ \
            "movl %%eax,%1\n\t"                                 /* 将AX和DX的组合值写入门描述符低4字节 */ \
            "movl %%edx,%2"                                     /* 将DX和高16位地址写入门描述符高4字节 */ \
            :                                                   /* 无输出操作数 */ \
            : "i"((short)(0x8000 + (dpl << 13) + (type << 8))), /* 门描述符属性: P=1, DPL, 类型 */ \
              "o"(*((char *)(gate_addr))),                      /* 门描述符低4字节地址 */ \
              "o"(*(4 + (char *)(gate_addr))),                  /* 门描述符高4字节地址 */ \
              "d"((char *)(addr)), "a"(0x00080000))             /* DX=处理程序地址低16位, AX=0x00080000(代码段选择子) */

/* 设置中断门(特权级0) */
#define set_intr_gate(n, addr) \
    _set_gate(&idt[n], 14, 0, addr)  /* 类型14表示中断门, DPL=0(仅内核可访问) */

/* 设置陷阱门(特权级0) */
#define set_trap_gate(n, addr) \
    _set_gate(&idt[n], 15, 0, addr)  /* 类型15表示陷阱门, DPL=0(仅内核可访问) */

/* 设置系统门(特权级3) */
#define set_system_gate(n, addr) \
    _set_gate(&idt[n], 15, 3, addr)  /* 类型15表示陷阱门, DPL=3(用户态可访问) */

/*
 * 设置段描述符
 * gate_addr: 段描述符地址
 * type: 段类型
 * dpl: 描述符特权级
 * base: 段基地址
 * limit: 段限长
 */
#define _set_seg_desc(gate_addr, type, dpl, base, limit)     \
    {                                                        \
        *(gate_addr) = ((base) & 0xff000000) |               /* 段基地址高8位 */ \
                       (((base) & 0x00ff0000) >> 16) |       /* 段基地址中8位 */ \
                       ((limit) & 0xf0000) |                 /* 段限长高4位 */ \
                       ((dpl) << 13) |                       /* 描述符特权级 */ \
                       (0x00408000) |                        /* G=1(4KB粒度), D=1(32位段), P=1(存在) */ \
                       ((type) << 8);                        /* 段类型 */ \
        *((gate_addr) + 1) = (((base) & 0x0000ffff) << 16) | /* 段基地址低16位 */ \
                             ((limit) & 0x0ffff);            /* 段限长低16位 */ \
    }

/*
 * 设置TSS或LDT描述符
 * n: 描述符地址
 * addr: TSS或LDT的基地址
 * type: 描述符类型(0x89 for TSS, 0x82 for LDT)
 */
#define _set_tssldt_desc(n, addr, type)              \
    __asm__("movw $104,%1\n\t"                       /* TSS/LDT段限长为104字节 */ \
            "movw %%ax,%2\n\t"                       /* 存储基地址低16位 */ \
            "rorl $16,%%eax\n\t"                     /* 将EAX中的基地址高16位移到低16位 */ \
            "movb %%al,%3\n\t"                       /* 存储基地址第24-16位 */ \
            "movb $" type ",%4\n\t"                  /* 设置描述符类型 */ \
            "movb $0x00,%5\n\t"                      /* 清除该字节(未使用) */ \
            "movb %%ah,%6\n\t"                       /* 存储基地址第31-24位 */ \
            "rorl $16,%%eax" ::"a"(addr),            /* EAX = TSS/LDT基地址 */ \
            "m"(*(n)), "m"(*(n + 2)), "m"(*(n + 4)), /* 操作数: 描述符各个字节 */ \
            "m"(*(n + 5)), "m"(*(n + 6)), "m"(*(n + 7)))

/* 设置TSS描述符 */
#define set_tss_desc(n, addr) _set_tssldt_desc(((char *)(n)), ((int)(addr)), "0x89")
/* 设置LDT描述符 */
#define set_ldt_desc(n, addr) _set_tssldt_desc(((char *)(n)), ((int)(addr)), "0x82")