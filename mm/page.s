/*
 *  linux/mm/page.s
 *  页面异常（缺页和写保护）低级处理汇编代码
 *  (C) 1991  Linus Torvalds
 */

/*
 * page.s 包含底层的页面异常处理代码
 * 实际工作在 mm.c 中完成
 */

// 声明全局符号，供中断描述符表使用
.globl page_fault

// 页面异常处理入口点（中断号14）
page_fault:
	// 交换eax和栈顶值（异常错误码）
	// 错误码包含异常原因信息：
	// 位0: 0-页面不存在，1-写保护违规
	// 位1: 0-超级用户模式，1-用户模式
	// 位2: 0-读操作，1-写操作
	xchgl %eax,(%esp)
	
	// 保存可能被使用的寄存器
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	
	// 设置内核数据段选择子(0x10)
	movl $0x10,%edx
	mov %dx,%ds
	mov %dx,%es
	mov %dx,%fs
	
	// 获取引起页错误的线性地址（存储在CR2寄存器中）
	movl %cr2,%edx
	
	// 将错误地址和错误码压栈，作为do_no_page/do_wp_page的参数
	pushl %edx          // 错误地址
	pushl %eax          // 错误码
	
	// 测试错误码的最低位，判断是缺页还是写保护错误
	testl $1,%eax
	jne 1f              // 如果位0=1（写保护），跳转到标签1
	
	// 处理缺页异常（页面不存在）
	call do_no_page     // 调用C函数处理缺页
	jmp 2f              // 跳转到公共返回路径
	
// 处理写保护异常
1:	call do_wp_page     // 调用C函数处理写保护页

// 公共返回路径
2:	addl $8,%esp        // 清理栈上的两个参数（错误地址和错误码）
	
	// 恢复保存的寄存器
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	
	// 中断返回，恢复中断前的执行
	iret