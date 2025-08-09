/*
 *  linux/kernel/asm.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * asm.s contains the low-level code for most hardware faults.
 * page_exception is handled by the mm, so that isn't here. This
 * file also handles (hopefully) fpu-exceptions due to TS-bit, as
 * the fpu must be properly saved/resored. This hasn't been tested.
 */
/**
; asm.s程序中国包括大部分硬件故障(或出错)处理的底层代码。
; 页异常是由内存管理程序mm处理的，所以不在这里处理。
; 此程序还处理由于TS位而造成的fpu异常，
; 因为fpu必须正确的进行保存、恢复处理，这些还没有进行测试。


; 本代码文件主要设计对intel保留的中断int0~int16的处理(int17-int31留作今后使用)。
; 以下是一些全局函数名的声明，其原型在traps.c中说明。

.globl divide_error,debug,nmi,int3,overflow,bounds,invalid_op
.globl double_fault,coprocessor_segment_overrun
.globl invalid_TSS,segment_not_present,stack_segment
.globl general_protection,coprocessor_error,irq13,reserved
; int 0
; 下面是被零除出错（divide_error）处理代码。标号“divide_error”实际上是C语言函数
; divide_error()编译后所生成的模块中对应的名称。'_do_divide_error'函数在traps.c中。

divide_error:
	pushl $do_divide_error;首先将要调用的函数名入栈，这段程序的出错号是0
no_error_code:;这里是无出错号处理的入口处，见下面第55行等。
	xchgl %eax,(%esp);_do_divide_error的地址->eax，eax被交换入栈。
    ; 保存其他寄存器
    pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi
	pushl %ebp
	push %ds;！！16位的段寄存器入栈后也要占用4个字节。
	push %es
	push %fs
	pushl $0		# "error code"将出错号入栈。
	lea 44(%esp),%edx #取原调用返回地址处堆栈指针位置，并压入堆栈。 
	pushl %edx
	movl $0x10,%edx # 内核代码数据段选择符
	mov %dx,%ds
	mov %dx,%es
	mov %dx,%fs
	call *%eax # 间接调用，例如调用C函数do_divide_error()
	addl $8,%esp # 让堆栈指针重新指向寄存器fs入栈处。

    # 恢复其他寄存器
	pop %fs 
	pop %es
	pop %ds
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret

debug: # int1 --debug调试中断入口点。处理过程如上。
	pushl $do_int3		# _do_debugC函数指针入栈，下同。
	jmp no_error_code

nmi: # int 2 --非屏蔽中断调用入口点。
	pushl $do_nmi
	jmp no_error_code

int3: # int 3 --断点指令引起中断的入口点，处理过程同_debug。
	pushl $do_int3
	jmp no_error_code

overflow: # int 4 --溢出中断调用入口点。
	pushl $do_overflow
	jmp no_error_code

bounds: # int 5 --越界中断调用入口点。
	pushl $do_bounds
	jmp no_error_code

invalid_op: # int 6 --无效操作指令中断调用入口点。
	pushl $do_invalid_op
	jmp no_error_code

coprocessor_segment_overrun: # int 9 --协处理器段错误中断调用入口点。
	pushl $do_coprocessor_segment_overrun
	jmp no_error_code

reserved: # int 15 --中断保留调用入口点。
	pushl $do_reserved
	jmp no_error_code

    ; 用于当协处理器执行完操作时就会发出irq13中断信号，以通知CPU操作完成。
irq13: # int 45 --(=0x20+13)数学协处理器发出的中断。
	pushl %eax
	xorb %al,%al #80387在执行计算时，CPU就会等待其操作的完成。
	outb %al,$0xF0
# 上句通过写0xF0端口，本中断消除CPU的BUSY延续信号，并重新激活387的处理器扩展请求引脚PEREQ。
# 该操作主要是为了确保在继续执行387的任何指令之前，响应本中断。
	movb $0x20,%al
	outb %al,$0x20 # 向8259主中断控制芯片发送EOI(中断结束)信号。
	jmp 1f         # 这两个跳转指令起到的延时作用。
1:	jmp 1f
1:	outb %al,$0xA0 # 再向8259从中断控制芯片发送EOI(中断结束)信号。
	popl %eax
	jmp coprocessor_error # _coprocessor_error原来在本文件中,现在已经放到(kernel/system_call.s，131)

double_fault:
	pushl $do_double_fault # C函数地址入栈
error_code:
	xchgl %eax,4(%esp)		# error code <-> %eax ,eax原来的值被保存在堆栈上。
	xchgl %ebx,(%esp)		# &function <-> %ebx,ebx原来的值被保存在堆栈上。
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi
	pushl %ebp
	push %ds
	push %es
	push %fs
	pushl %eax			# error code 出错号入栈。
	lea 44(%esp),%eax		# offset 程序返回地址处堆栈指针位置值入栈。
	pushl %eax
	movl $0x10,%eax # 置内核数据段选择符
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	call *%ebx #调用响应的C函数，其参数已入栈。
	addl $8,%esp # 堆栈指针重新指向栈中放置fs内容的位置。
	pop %fs
	pop %es
	pop %ds
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret

invalid_TSS: #int10——无效的任务状态段(TSS)
	pushl $do_invalid_TSS
	jmp error_code

segment_not_present:# int11——段不存在
	pushl $do_segment_not_present
	jmp error_code

stack_segment:# int12——堆栈段错误
	pushl $do_stack_segment
	jmp error_code

general_protection: #int13——一般保护性出错。
	pushl $do_general_protection
	jmp error_code

    
# int7——设备不存在(_device_not_available),在kernel/system_call.s,148行。
# int14——页错误(page_fault),在mm/page.s，14行。
# int16—— 协处理器错误(_coprocessor_error),在system_call.s,131行。
# 时钟中断int 0x20(_time_interrupt)和系统调用int0x80(_system_call)在system_call.s中。

