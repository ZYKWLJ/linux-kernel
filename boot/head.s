#startup_32 (entry)
#├── 初始化段寄存器 (DS/ES/FS/GS = 0x10)
#├── 设置堆栈 (lss stack_start, %esp)
#├── 调用 setup_idt (设置中断描述符表 IDT)
#├── 调用 setup_gdt (设置全局描述符表 GDT)
#├── 重新加载段寄存器 (更新 GDT 后)
#├── 再次设置堆栈 (确保 SS 指向新 GDT 中的段)
#├── 检测 A20 地址线是否开启
#├── 检测数学协处理器 (check_x87)
#├── 跳转到 after_page_tables
#│   ├── 压入 main 函数参数及返回地址
#│   └── 调用 setup_paging (初始化分页机制)
#│       └── 执行完毕后通过 ret 跳转至 main 函数
#└── 进入内核主逻辑 (main 函数)

/*
 *  linux/boot/head.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  head.s contains the 32-bit startup code.
 *
 * NOTE!!! Startup happens at absolute address 0x00000000, which is also where
 * the page directory will exist. The startup code will be overwritten by
 * the page directory.
 */

.text
.globl idt,gdt,pg_dir,tmp_floppy_area
pg_dir: #页目录将会存放在这里
.globl startup_32 
startup_32:
	movl $0x10,%eax #段选择子二进制为：00000000 00000000 00000000 00010000

# 段选择子的结构
#高13位           1位    2位
#+---------------+-----+-----+
#| 描述符索引     | TI  | RPL |
#+---------------+-----+-----+
# 这里的段选择子0x10构成为:特权级RPL=0,TI=0意为选择全局描述符表GDT，选择项数为表中的第(3~15位)0x00010=2项。正好指向了全局段描述符中的数据段描述符项。
# 描述符的具体数值为setup.s中指定的0x07FF。

# 段描述符的结构
# Byte 7       Byte 6       Byte 5       Byte 4       Byte 3       Byte 2       Byte 1       Byte 0
# +------------+------------+------------+------------+------------+------------+------------+------------+
# |G D/B 0 AVL | Limit 19-16 | P DPL S TYPE | Limit 15-0 |
# |            |             |              |            |
# +------------+------------+------------+------------+------------+------------+------------+------------+
# |          Segment Base 31-24           |       Segment Base 23-16       |
# +--------------------------------------+--------------------------------+
# |                 Segment Base 15-0                        |
# +----------------------------------------------------------+

# 下面4句是将段选择子设置进各个段寄存器里面，设置多个段寄存器可以确保无论使用哪个段寄存器访问数据，寻址空间都是一致的。
	mov %ax,%ds #数据段寄存器
	mov %ax,%es #附加段寄存器
	mov %ax,%fs #附加段寄存器
	mov %ax,%gs #附加段寄存器

# 表示_stack_start->ss:esp,第一次设置系统堆栈。
# 此时旧的堆栈环境（实模式下的堆栈）已失效，需要建立保护模式下的新堆栈。
# 这个堆栈将会放置在_stack_start处,这是在`kernel/sched.c`中定义的。

	lss stack_start,%esp
# 调用设置中断描述符表。
	call setup_idt
# 调用设置全局描述符表。
	call setup_gdt  # 这里面会设置好CS


# 这里值得深思，为什么这里需要重新加载段寄存器？因为之前的临时段寄存器信息已过时，上面调用了函数重新设置了gdt\idt，所以要重新设置段寄存器
# 其实也显然，因为之前的临时段寄存器设置时还是在0x90200处执行的！已经过时！   

	movl $0x10,%eax	# reload all the segment registers
	mov %ax,%ds		# after changing gdt. CS was already(不需要加载CS，因为在setup_gdt中已经设置好了)
	mov %ax,%es		# reloaded in 'setup_gdt'
	mov %ax,%fs     ，
	mov %ax,%gs

# 表示_stack_start->ss:esp,第二次设置系统堆栈。
# 此时gdt更新了，旧缓存的 SS 描述符（如基址、权限）可能与新 GDT 不一致，需通过 lss 重新加载新GDT中的堆栈段描述符。
# 同样这个堆栈将会放置在_stack_start处,这是在`kernel/sched.c`中定义的。
	lss stack_start,%esp

# 用于检测A20是否开启.采用的方法是检测是否发生了地址环绕。
# 如果没开启，那么会发生地址环绕，也就是[0x100000]=[0x000000].这就是判断依据。

    xorl %eax,%eax
1:	incl %eax		# check that A20 really IS enabled
	movl %eax,0x000000	# loop forever if it isn't
	cmpl %eax,0x100000
	je 1b # 如果相等，就跳转到1b处，也就是标签1处。

/*
 * NOTE! 486 should set bit 16, to check for write-protect in supervisor
 * mode. Then it would be unnecessary with the "verify_area()"-calls.
 * 486 users probably want to set the NE (#5) bit also, so as to use
 * int 16 for math errors.
 */

# 下面这段程序对数学协处理器芯片是否存在。方法是修改控制寄存器CR0，在假设存在协处理器的情况下执行一个协处理器指令，如果出错的话，则说明协处理器芯片不存在
# 则需要设置CR0中的协处理器仿真位EM(位2)，并复位协处理器存在标志MP(位1)。

	movl %cr0,%eax		# check math chip
	andl $0x80000011,%eax	# Save PG,PE,ET
/* "orl $0x10020,%eax" here for 486 might be good */
	orl $2,%eax		# set MP
	movl %eax,%cr0
	call check_x87
	jmp after_page_tables

/*
 * We depend on ET to be correct. This checks for 287/387.
 */
check_x87:
	fninit
	fstsw %ax
	cmpb $0,%al
	je 1f			/* no coprocessor: have to set bits */
	movl %cr0,%eax
	xorl $6,%eax		/* reset MP, set EM */
	movl %eax,%cr0
	ret


.align 2 
# 这里的".align 2"的含义是指存储边界对齐调整。“2”表示调整到地址最后2位为0，即按4字节方式对齐内存地址(显然了，100肯定为4的倍数)！ 
# 对齐的主要目的是提高CPU的寻址运行效率。

# 287协处理器码
# 协处理器是用于协助CPU计算的机器，专注于某一个模块，例如附点运算、图像处理等等。
# 1991年的单张CPU计算能力差，所以需要协处理器来辅助计算。
# 协处理器是独立于CPU的，它的存在不会影响CPU的正常运行。

1:	.byte 0xDB,0xE4		/* fsetpm for 287, ignored by 387 */
	ret

/*
 *  setup_idt
 *
 *  sets up a idt with 256 entries pointing to
 *  ignore_int, interrupt gates. It then loads
 *  idt. Everything that wants to install itself
 *  in the idt-table may do so themselves. Interrupts
 *  are enabled elsewhere, when we can be relatively
 *  sure everything is ok. This routine will be over-
 *  written by the page tables.
 */

# 这段是设置中断描述符子程序，刚刚主程序在第一次加载了段寄存器后，就开始了这里的设置。
# 第一次本质上还是setup.s程序里面设置的段寄存器，但是这里真正在内核中还需要自己的重新设置之！

setup_idt:
	lea ignore_int,%edx

# 这里的结构是将0008放入eax的高16位中，也就是0000 0000 0000 1000 
# 按照段选择子的 13  1 2 法则，这是指的是特权级为0的全局描述符表中的第1项！

	movl $0x00080000,%eax # 将选择符0x0008置入eax的高16位中。
	movw %dx,%ax		/* selector = 0x0008 = cs 表示：中断处理程序的代码`段选择子`与`当前代码段寄存器`（CS）的值相同，都指向 `GDT 中的内核代码段`。*/
	movw $0x8E00,%dx	/* interrupt gate - dpl=0, present */

	lea idt,%edi # idt是中断描述符表的地址
	mov $256,%ecx

rp_sidt:
	movl %eax,(%edi) #将哑中断门描述符存入表中！哑中断描述符是指只报错误的中断符！
	movl %edx,4(%edi)
	addl $8,%edi # edi指向下一项(+8B,说明每一项有8字节)
	dec %ecx
	jne rp_sidt
	lidt idt_descr #加载中断描述符表寄存器值(lidt指令专用于加载中断描述符表寄存器的!)
	ret

/*
 *  setup_gdt
 *
 *  This routines sets up a new gdt and loads it.
 *  Only two entries are currently built, the same
 *  ones that were built in init.s. The routine
 *  is VERY complicated at two whole lines, so this
 *  rather long comment is certainly needed :-).
 *  This routine will beoverwritten by the page tables.
 */

# 设置全局描述符表项setup_gdt
# 这里的子程序设置一个新的全局描述符表gdt，并加载。此时仅创建了两个表项，与前面的一样！

setup_gdt:
	lgdt gdt_descr # 加载全局描述符表寄存器(此处在后面定义)
	ret

/*
 * I put the kernel page tables right after the page directory,
 * using 4 of them to span 16 Mb of physical memory. People with
 * more than 16MB will have to expand this.
 */

 # linus将内核的内存页直接放在页目录之后，使用了4个表来寻址16MB的物理内存。
 # 每个页表长4KB字节，而每个也表项需要4个字节，因此一个页表共可以存放1024个表项，如果一个表项寻址4KB的地址空间，
 # 那么一个页表可以寻址4MB的物理内存。

# 从偏移0x1000处开始是第一个页表(偏移0开始处将存放页表目录，所以第一个页表不是0x00000)
.org 0x1000
pg0:

# 从偏移0x2000处开始是第二个页表，以下依次类推，存放第三第四个页表！(linux0.11只设置了4个页表!)
.org 0x2000
pg1:

.org 0x3000
pg2:

.org 0x4000
pg3:

.org 0x5000
/*
 * tmp_floppy_area is used by the floppy-driver when DMA cannot
 * reach to a buffer-block. It needs to be aligned, so that it isn't
 * on a 64kB border.
 */

# 这里的设置意义：如果DMA(直接存储器访问)不能访问缓冲块时，下面的tmp_floppy_area内存块就可供软盘驱动程序使用。
# 其地址需要对其调整，这样就不会跨越64KB边界。
tmp_floppy_area:
	.fill 1024,1,0

# 这是设置的页表之后内存，为调用/init/main.c程序和返回做准备。

after_page_tables: # 其实这里的名字也一目了然了！

# 下面都是调用main函数的参数
# x86中参数通过栈传递，顺序`从右到左`

	pushl $0        # 第4个参数（通常 unused）
	pushl $0        # 第3个参数（通常 unused）
	pushl $0        # 第2个参数（通常 unused）
	pushl $L6       # 第1个参数：main函数的返回地址（若main返回会跳转到L6）

# 准备调用main函数，将main函数地址压入栈顶中，作为setup_paging执行后的`返回地址`！

	pushl $main  


# 调用setup_paging子程序，负责初始化分页机制(如设置页表、启动分页)
# setup_paging执行完了后，会通过ret指令返回到栈顶保存的地址(即main函数)，从而启动内核的主逻辑！
	
    jmp setup_paging 

# 下面的`jmp L6`是如果main函数返回了，那么就跳转到L6处，这是一个死循环。
# 这实际上是一个安全措施！
L6:
	jmp L6			# main should never return here, but
				# just in case, we know what happens.


# 这是默认的中断处理向量句柄！

/* This is the default interrupt "handler" :-) */
int_msg:
	.asciz "Unknown interrupt\n\r" # 未知中断
.align 2 # 按4字节对齐(100结尾一定是4字节的倍数!)

ignore_int: # 默认中断

    #保存寄存器上下文：保存 CPU 寄存器状态 (EAX, ECX, EDX, DS, ES, FS)
#为什么保存这些寄存器？
#EAX/ECX/EDX：通用寄存器，可能被 printk 函数修改
#DS/ES/FS：段寄存器，指向不同的数据段

	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs

# 置段选择符(使得ds、es、fs段寄存器指向内核gdt表中的数据段)
	movl $0x10,%eax #0x10 是内核数据段的段选择符 (Selector)
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	pushl $int_msg # 把调用printk函数的参数指针(地址)入栈
	call printk # 调用printk函数，此为内核打印函数，类似用户空间的 printf
	popl %eax #popl % eax 平衡堆栈，移除参数

# 中断处理程序末尾恢复寄存器是为了确保中断返回后，被中断的程序能够继续正确执行。
# 回到中断处理程序之前的状态(敲门的事处理完了，现在重新看书)

	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret


/*
 * Setup_paging
 *
 * This routine sets up paging by setting the page bit
 * in cr0. The page tables are set up, identity-mapping
 * the first 16MB. The pager assumes that no illegal
 * addresses are produced (ie >4Mb on a 4Mb machine).
 *
 * NOTE! Although all physical memory should be identity
 * mapped by this routine, only the kernel page functions
 * use the >1Mb addresses directly. All "normal" functions
 * use just the lower 1Mb, or the local data space, which
 * will be mapped to some other place - mm keeps track of
 * that.
 *
 * For those with more memory than 16 Mb - tough luck. I've
 * not got it, why should you :-) The source is here. Change
 * it. (Seriously - it shouldn't be too difficult. Mostly
 * change some constants etc. I left it at 16Mb, as my machine
 * even cannot be extended past that (ok, but it was cheap :-)
 * I've tried to show which constants to change by having
 * some kind of marker at them (search for "16Mb"), but I
 * won't guarantee that's all :-( )
 */

 #这个子程序通过设置控制寄存器cr0的标志(PG位31)来启动堆内存的`分页处理功能`，并设置各个`页表项`的内容，
 #以恒等映射前16MB的物理内存。

 # 注意！尽管这个子程序通过恒等映射了所有的物理内存，但是`内核页函数``只`使用了`大于1Mb`的地址。
 # 所有“普通”函数只使用了`1Mb`以下的地址，或者是`局部数据空间`，它会被映射到其他地方——mm(通过内存管理程序)会管理这些事！


 # 虚拟地址的组成
 # 虚拟地址 = [页目录索引(10位)] + [页表索引(10位)] + [页内偏移(12位)]
# +---------------------------------- 32位虚拟地址 ----------------------------------+
# |  页目录索引(10位)   |   页表索引(10位)    |         页内偏移(12位)                 |
# +---------------------------------- 32位虚拟地址 ----------------------------------+

# 我们通过页目录索引、页表索引拿到物理地址的高20位，然后在加上虚拟地址的页内偏移，就得到了物理地址！

#========================================================================================================================================================================================================
# 到这里我们基本就全部明白了，我们通过`段选择子`拿到全部描述符表的索引，找到描述符，拿到了`段基址`，加上从ip寄存器里面拿到的有效地址EA，得到段`偏移量`，就得到了`虚拟地址`！
# 有了虚拟地址后，我们就可以通过`MMU(内存管理单元)`将虚拟地址转换为物理地址了！具体的转换方式是，通过页表的索引，拿到物理地址的高20位(左移12位构成物理地址的高20位)，然后加上虚拟地址的页内偏移(12位)，就得到了物理地址！
#========================================================================================================================================================================================================

.align 2

# 首先对5页内存(1页目录+4页页表)清零。
setup_paging:
	movl $1024*5,%ecx		/* 5 pages - pg_dir+4 page tables */
# 清零寄存器	
    xorl %eax,%eax 
	xorl %edi,%edi			/* pg_dir is at 0x000 */ # 页目录从0x000地址开始。
	cld;rep;stosl
# =============================================================================
# 下面4句设置页目录中的项，我们共有4个页表，所以只需要设置4项。
# 页目录项和页表项的结构一样，4B为1项。

# 其中页目录中的第一项表示指向第一个页表，"$pg0+7"表示页表0的基地址(0x1000)，加上7为：0x0000 1007, 表示设置了`存在位`、`用户态`、`读写权限`。
# 则第一个页表的所在地址为：0x0000 1007&0xfffff000=0x1000;
# 第一个页表的属性标志为：0x0000 1007&0x0000 0fff=0x0000 0007, 表示该页存在、用户可读写。

# 这里的页表项(页表目录项也一样的)结构：
# +------------------------------+---------------------+--------------+-----------------+--------------+---------------+------------+
# | 页框基地址(12-32，共20位)     | 辅助位(7-11，共5位)  | D(6，共1位)   | 保留(3-5，共3位) | U/S(2，共1位) | R/W(1，共1位) | P(0，共1位) |
# +------------------------------+---------------------+--------------+-----------------+--------------+---------------+------------+

# 其中:
# P为存在位，代表也是否存在于物理内存中。
# R/W代表读写许可
# U/S代表用户态/内核态
# D代表是否修改后(是否脏了)
# 辅助位用于缓存策略等
# 页框基地址代表一页内存的物理起始地址

    movl $pg0+7,pg_dir		/* set present bit/user r/w */
	movl $pg1+7,pg_dir+4		/*  --------- " " --------- */
	movl $pg2+7,pg_dir+8		/*  --------- " " --------- */
	movl $pg3+7,pg_dir+12		/*  --------- " " --------- */

# =========================================================================
# 下面6行填写4个页表中的所有项的内容，共有4(页表)*1024(项/页表)=4096项，每项能辐射的物理内存为4KB，那么总共能辐射的物理内存为：4096*4KB=16MB。

# 每项的内容：当前项的所映射的物理地址+页标志位(存在位+用户态+读写权限)(这里均为7，也就是0111)
	movl $pg3+4092,%edi # $pg3=0x40000, 这里因为从后面设置来，一个页表共1024项，一项4B，那么一个表就是4096B，所以最后一项的起始地址是0x4092.
	movl $0xfff007,%eax	# 获得了起始地址后，将该项设置的内容定为0xfff007,这也有大讲究，如下：	/*  16Mb - 4096 + 7 (r/w user,p) */

# 0xfff007 = 0b1111 1111 1111 0000 0000 0000 0111
# 物理地址 = 0x00FFF0 << 12 = 0x00FFF0000 = 0xFFF00000
# 为什么需要左移12位？因为在Linux中，物理地址的起始页一定是4KB的整数倍，所以低12位永远为0，那么我们就没有必要存储低12位，只需要存储高20位。
# 这也是为什么页表框基地址只需要存储高20位即可。
# 所以我们需要将页表中的地址映射左移12位，再加上虚拟地址的低12位的偏移，就组成了32位的物理地址。

	std
# stosl指令详解

# 这是x86 汇编中的`stosl`指令，它是串存储指令（Store String）的一种，用于高效地将`数据`从`寄存器`存储到`内存`中。

# 1. [EDI] = EAX（将 EAX 的值写入 EDI 指向的内存） 
# 2. EDI = EDI ± 4（根据 DF 标志决定加减：DF=0 则 + 4，DF=1 则 - 4）

1:	stosl			/* fill pages backwards - more efficient :-) */
	subl $0x1000,%eax # 每填写一项就物理地址就减去0x1000，直到eax=0为止。
	jge 1b # 如果小于0，则说明全填写好了！否则继续循环！

# 直到这里，都是进行页表项的设置，设置了4096项
# ============================================================================

# 下面的代码是将页目录地址写入控制寄存器cr3中，然后将cr0寄存器的PG位设置为1，启动分页机制。

	cld
	xorl %eax,%eax		/* pg_dir is at 0x0000 */ # 置零
	movl %eax,%cr3		/* cr3 - page directory start */ # 将页目录地址写入控制寄存器cr3中，显然页目录地址为0x00000
	movl %cr0,%eax # 启动分页处理机制前，将除了PG标志外的其他标志都存在%eax中。
	orl $0x80000000,%eax # 添上PG标志。
	movl %eax,%cr0		/* set paging (PG) bit */ # 将cr0寄存器的PG位设置为1，启动分页机制。

# 在改变分页处理标志后，需要使用转移指令刷新预处理指令队列，这里使用返回指令ret。
# 此外，此返回指令的另一个作用是将栈顶的main程序地址弹出，并开始运行/init/main.c程序。

#+------------------------+
#| 至此，本程序真正结束了！ |
#+------------------------+

	ret			/* this also flushes prefetch-queue */

# ======================================================

.align 2
.word 0

idt_descr:
# 下面两行是 lidt 指令的 6B 操作数:长度,基址。

	.word 256*8-1		# idt contains 256 entries(每个8字节，所以总的是8*256字节)
	.long idt

.align 2
.word 0

# 这就是前面提到的获取全局描述符表寄存器的方法。
gdt_descr: 
# 下面两行是 lgdt 指令的 6B 操作数:长度,基址。

	.word 256*8-1	# so does gdt (not that that's any
	.long gdt		# magic number, but it works for me :^)

	# .align 8(这里应该是作者写错了，应该是3，已改正在下面，按照8字节对齐嘛!)
	.align 3

# 拥有256项的中断描述符表，每项8字节，均填充为0。

idt:	.fill 256,8,0		# idt is uninitialized

# 拥有256项的全局描述符表，每项8字节。
# 其中前四项分别是：空项(不用)、代码段描述符、数据段描述符、系统段描述符。
# 其中，代码段描述符负责内核代码执行：覆盖整个 4GB 地址空间，允许内核态执行可执行代码
# 数据段描述符负责内核数据读写：覆盖整个 4GB 地址空间，允许内核态读写数据
# 
# 其中系统段描述符表在linux0.11中没有使用，后面还预留了252项，用于存放其他用户层创建任务的局部描述符表(LDT)和对应的任务状态段(TSS)的描述符。
# 具体来说，全局段描述符表的结构如下：
# 0-NULL,1-cs,2-ds,3-sys,4-TSS0,5-LDT0,6-TSS1,7-LDT1,8-TSS2....
# Global Descriptor Table (GDT) Layout:

#+--------+--------+--------+--------+--------+--------+--------+--------+-----------------------------------+
#|  NULL  |  CS    |  DS    |  SYS   |  TSS0  |  LDT0  |  TSS1  |  LDT1  |                   ...             |
#|  (0)   |  (1)   |  (2)   |  (3)   |  (4)   |  (5)   |  (6)   |  (7)   |     (剩下的252项，重复的TSS和LDT)   |
#+--------+--------+--------+--------+--------+--------+--------+--------+-----------------------------------+



gdt:	.quad 0x0000000000000000	/* NULL descriptor */ # 空描述符。
	.quad 0x00c09a0000000fff	/* 16Mb */ # 代码段最大的长度16MB。
	.quad 0x00c0920000000fff	/* 16Mb */ # 数据段最大的长度16MB。
	.quad 0x0000000000000000	/* TEMPORARY - don't use */ # 系统段描述符暂时不用。
	.fill 252,8,0			/* space for LDT's and TSS's etc */ # 剩下的预留252项条目。

