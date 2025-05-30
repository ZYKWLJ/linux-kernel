	.code16
# original version is intel，rewrite with AT&T syntax 
#
# SYS_SIZE is the number of clicks (16 bytes) to be loaded.
# 0x3000 is 0x30000 bytes = 196kB, more than enough for current
# versions of linux
#
# SYSSIZE是bootsect要加载的节数(即编译后要加载的system的系统的大小)。
# 一节等于16B，这里按照当时的换算，1kb=1000b来算的话，这里有0x30000字节=196kb。
# 如果是1kb=1024b的换算的话，这里有3*（16^3）=12*（2^10）=12kb节，换为字节，就是12*16=192kb。不管哪种，都足够了。
	.equ SYSSIZE, 0x3000
#
#	bootsect.s		(C) 1991 Linus Torvalds
#
# bootsect.s is loaded at 0x7c00 by the bios-startup routines, and moves
# iself out of the way to address 0x90000, and jumps there.
#
# It then loads 'setup' directly after itself (0x90200), and the system
# at 0x10000, using BIOS interrupts. 
#
# NOTE! currently system is at most 8*65536 bytes long. This should be no
# problem, even in the future. I want to keep it simple. This 512 kB
# kernel size should be enough, especially as this doesn't contain the
# buffer cache as in minix
#
# The loader has been made as simple as possible, and continuos
# read errors will result in a unbreakable loop. Reboot by hand. It
# loads pretty fast by getting whole sectors at a time whenever possible.

# bootsect.s被bios启动子程序加载到0x7c00(31kb)处，然后将自己移动到0x90000(576kb)处，并跳转到哪里去执行。
# 然后，它在使用BIOS中断将`setup模块(由setup.s编译而来)`直接加载到自己的后面，即0x90200(592kb)处，并将system模块加载到0x10000(64kb)处。

# 一一对应的6个全局标识符，其中_start是程序的入口点
	.global _start, begtext, begdata, begbss, endtext, enddata, endbss 
	.text       # 切换到文本段(这是实际机器码的存储位置)
	begtext:    # 标记文本段的起始地址
	.data       # 切换到数据段
	begdata:    # 标记数据段的起始地址
	.bss        # 切换到BSS(未初始化的数据)段
	begbss:     # 标记BSS段的起始地址
	.text       # 最后切换回文本段（可能用于后续代码）

# setup模块的大小为4个扇区。
	.equ SETUPLEN, 4		# nr of setup-sectors

# bootsect模块的原始段地址为0x7c0.
# (注意这里为什么是0x7c0，而不是0x7c00，因为在编译时，实际地址是段地址<<4+偏移地址，而4位刚好是16进制下的1位，故这里看着就像少了一位一样，即0x7c0)

	.equ BOOTSEG, 0x07c0		# original address of boot-sector

# 移动bootsect模块后，其段地址为应为0x9000.
	.equ INITSEG, 0x9000		# we move boot here - out of the way

# setup模块的起始段地址为0x9020.
	.equ SETUPSEG, 0x9020		# setup starts here

# system模块的起始段地址为0x1000.(64KB)
	.equ SYSSEG, 0x1000		# system loaded at 0x10000 (65536).

# system模块的结束(停止加载)段地址为0x1000+SYSSIZE=0x1000+0x3000=0x4000(128KB)。
	.equ ENDSEG, SYSSEG + SYSSIZE	# where to stop loading

# ROOT_DEV:	0x000 - same type of floppy as boot.
#		0x301 - first partition on first drive etc
#
# #和源码不同，源码中是0x306 第2块硬盘的第一个分区
# 这里改为0x301，即第一个硬盘的第一个分区。
# 为何这么修改？

# ROOT_DEV 是一个常量，用于指定`根文件系统`所在的磁盘分区。
# 源码中的 0x306：假设系统使用双硬盘，且根分区位于第二个硬盘（/dev/hdb1）。
# 修改后的 0x301：假设系统只有一个硬盘，且根分区位于第一个硬盘的第一个分区（/dev/sda1），这是更常见的配置。
# 此处的修改是为了适应不同的系统配置，使得代码更加通用。同时更加简化！
	.equ ROOT_DEV, 0x301
	ljmp    $BOOTSEG, $_start

# 程序的入口点
_start:
	mov	$BOOTSEG, %ax	#将ds段寄存器设置为0x7C0
	mov	%ax, %ds
	mov	$INITSEG, %ax	#将es段寄存器设置为0x900
	mov	%ax, %es
	mov	$256, %cx		#设置移动计数值256字

# 寄存器清零操作，将si和di寄存器清零，也就是偏移为0，基地址在前面的段寄存器里面已经指定了
	sub	%si, %si		#源地址	ds:si = 0x07C0:0x0000
	sub	%di, %di		#目标地址 es:si = 0x9000:0x0000
	rep					#重复执行并递减cx的值，直至cx=0
	movsw				#从内存[si]处移动cx个字到[di]处
	ljmp	$INITSEG, $go	#段间跳转，这里INITSEG指出跳转到的段地址，解释了cs的值为0x9000


# ====================================================
# 这里是一条分界线，接下来CPU就移动到0x9000段处的代码开始执行了
go:	mov	%cs, %ax		#将ds，es，ss都设置成移动后代码所在的段处(0x9000)
	mov	%ax, %ds
	mov	%ax, %es
# put stack at 0x9ff00.
	mov	%ax, %ss
	mov	$0xFF00, %sp		# arbitrary value >>512

# 注意这里的栈指针为0x9ff00的来源：

# 六段分布——栈大小预留：0x9FF00 - 0x90000 ≈ 64KB，足够容纳启动阶段的函数调用和局部变量。
# 16 字节对齐：0x9FF00 是 16 的倍数，符合栈对齐要求（提升内存访问效率）。
# 任意性说明：注释中提到 arbitrary value >>512，表示这个值是 “任意选择的，但远大于 512 字节”。实际上，只要不与其他内存区域冲突，具体数值可以调整。

# load the setup-sectors directly after the bootblock.
# Note that 'es' is already set up.

#
##ah=0x02 读磁盘扇区到内存	al＝需要独出的扇区数量
##ch=磁道(柱面)号的低八位   cl＝开始扇区(位0-5),磁道号高2位(位6－7)
##dh=磁头号					dl=驱动器号(硬盘则7要置位)
##es:bx ->指向数据缓冲区；如果出错则CF标志置位,ah中是出错码
#

# 开始加载setup代码了

load_setup:
	mov	$0x0000, %dx		# drive 0, head 0
	mov	$0x0002, %cx		# sector 2, track 0。扇区2，(从扇区2开始，因为扇区1是bootsect)。磁道0
	mov	$0x0200, %bx		# address = 512, in INITSEG。目标地址0x90200(INITSEG =0x9000,偏移=0x200)


# 这里通过 .equ 把 0x0200（ah=0x02 、al=0x00 ？不，实际是 ax 整体构造：ah 是 0x02（功能号），al 是扇区数 ） + SETUPLEN（扇区数）
# 最终让 ax 寄存器的值为 0x0200 + SETUPLEN ，即 ah=0x02（指定 BIOS 读第2个扇区）、al=SETUPLEN（要读的扇区数量为4），
# 这样执行 int 0x13 时，就能按需求读取对应数量的扇区到内存。

	.equ    AX, 0x0200+SETUPLEN
# 要读取的扇区数
	mov     $AX, %ax		# service 2, nr of sectors
# 调用BIOS中断0x13，开始读取扇区
	int	$0x13			# read it
	jnc	ok_load_setup		# ok - continue
	mov	$0x0000, %dx
# 错误处理：重置磁盘控制器并重试
	mov	$0x0000, %ax		# reset the diskette
	int	$0x13
	jmp	load_setup

ok_load_setup:

# Get disk drive parameters, specifically nr of sectors/track
# 获取磁盘参数（INT 0x13/AH=8）
	mov	$0x00, %dl# 驱动器号：0=第一个软盘
	mov	$0x0800, %ax		# AH=8 is get drive parameters，AH=8（获取驱动器参数）

# 调用BIOS磁盘服务
	int	$0x13
	mov	$0x00, %ch
	#seg cs
	mov	%cx, %cs:sectors+0	# %cs means sectors is in %cs
	mov	$INITSEG, %ax
	mov	%ax, %es

# Print some inane message
	mov	$0x03, %ah		# read cursor pos。AH=3（获取光标位置）
	xor	%bh, %bh 		# 页码置0
	int	$0x10

# 这里是加载system模块的显示字符的格式，显示的具体字符定义在下面，这里是设置了光标位置和颜色。

	mov	$89, %cx      		# 字符个数
	# mov	$0x0007, %bx		# page 0, attribute 7 (normal)# BH=0（页码），BL=7（白色文本，黑色背景）
	mov     $0x0004, %bx    # BH=0（页码），BL=0x04（红色前景/黑色背景）
	#lea	msg1, %bp
	mov     $msg1, %bp		# 字符串地址
	mov	$0x1301, %ax		# write string, move cursor，AH=13(写字符串)，AL=1(带属性，移除光标)
	int	$0x10 # 在屏幕上显示指定的加载字符！

# why $0x10?
# BIOS（基本输入输出系统）为不同的硬件设备和系统功能分配了特定的中断号：
# 0x00-0x0F：用于系统异常和错误处理（如除法错误、断点）。
# 0x10：视频服务（屏幕显示、文本输出、图形模式）。
# 0x13：磁盘服务（读写扇区、获取驱动器参数）。
# 0x16：键盘服务（读取按键）。
# 0x1A：时钟服务（获取系统时间）。

# ok, we've written the message, now
# we want to load the system (at 0x10000)
	mov	$SYSSEG, %ax
	mov	%ax, %es		# segment of 0x010000
# read_it读取磁盘上的system模块
	call	read_it 
# 关闭电动机，这样就可以知道驱动器状态了
	call	kill_motor

# After that we check which root-device to use. If the device is
# defined (#= 0), nothing is done and the given device is used.
# Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
# on the number of sectors that the BIOS reports currently.

	#seg cs
	mov	%cs:root_dev+0, %ax # 读取预设的根设备号
	cmp	$0, %ax # 检查是否指定根设备
	jne	root_defined # 已指定则跳转
	#seg cs
	# 未指定是，根据磁盘扇区自动检测
	mov	%cs:sectors+0, %bx # 获取每磁扇区数
	mov	$0x0208, %ax		# 默认设置为1.2MB 软盘 /dev/ps0 - 1.2Mb
	cmp	$15, %bx # 比较扇区数数是不是15
	je	root_defined # 若是则跳转
	mov	$0x021c, %ax		# 否则设置为1.44MB软盘 /dev/PS0 - 1.44Mb
	cmp	$18, %bx # 比较扇区数数是不是18
 	je	root_defined # 若是则跳转

undef_root:
	jmp undef_root # 无法识别则进入死循环
root_defined:
	# 在 AT&T 汇编语法中，#seg cs 是一个特殊的注释标记，用于指示汇编器将后续操作数解释为相对于代码段（CS）的地址。
	#seg cs
	mov	%ax, %cs:root_dev+0 # 保存最终确定的设备号

# after that (everyting loaded), we jump to
# the setup-routine loaded directly after
# the bootblock:

# 功能：执行段间远跳转，跳转到 SETUPSEG 段（物理地址 0x90200）的偏移 0 处。
# 目的：将控制权交给 Setup 模块，由其继续完成内核加载前的硬件初始化。

	ljmp	$SETUPSEG, $0

# This routine loads the system at address 0x10000, making sure
# no 64kB boundaries are crossed. We try to load it as fast as
# possible, loading whole tracks whenever we can.
#
# in:	es - starting address segment (normally 0x1000)
#


sread:	.word 1+ SETUPLEN	# sectors read of current track当前磁道已读扇区数
head:	.word 0			# current head 当前磁头号
track:	.word 0			# current track 当前磁道号

read_it:
	mov	%es, %ax
# 从盘上读入的数据必须存放在位于内存地址64KB的边界开始处，否则进入死循环
	test	$0x0fff, %ax
die:	jne 	die			# es must be at 64kB boundary
	xor 	%bx, %bx		# bx is starting address within segment

# 判断是不是读取完整了整个system的数据，如果是则返回，不是就跳转到ok1_read继续读取。
rp_read:
	mov 	%es, %ax
 	cmp 	$ENDSEG, %ax		# have we loaded all yet?
	jb	ok1_read
	ret


# ok1_read：
# 剩余扇区数是否超过当前内存段的剩余空间。如果超过，计算需要调整的扇区数（%ax）。
# ok2_read：调用磁盘读取函数，检查是否读完所有扇区。
# ok3_read：更新内存地址，处理段溢出。
# ok4_read：更新磁头和磁道编号，处理跨磁头 / 磁道的情况。



# ok1_read：
# 剩余扇区数是否超过当前内存段的剩余空间。如果超过，计算需要调整的扇区数（%ax）。

ok1_read:
	#seg cs
	mov	%cs:sectors+0, %ax # 将总扇区数加载到%ax
	sub	sread, %ax # 计算剩余扇区数（总扇区数-已读扇区数）
	mov	%ax, %cx # 将剩余扇区数保存到%cx
	shl	$9, %cx # 扇区数×512(2^9)，得到字节数(一个扇区512b)
	add	%bx, %cx # 计算当前内存地址+剩余字节数
	jnc ok2_read # 如果没有进位（内存未溢出），跳转到ok2_read
	je 	ok2_read # 如果结果为0，也跳转到ok2_read
	xor 	%ax, %ax # 否则（内存溢出），清零%ax
	sub 	%bx, %ax # 计算0-%bX,得到需要调整的字节数
	shr 	$9, %ax # 将字节数转换为扇区数


# ok2_read：
# 1.调用 read_track 函数读取扇区。
# 2.检查是否所有扇区都已读完。
# 3.如果读完，判断是否需要切换磁头或磁道。

ok2_read:
	call 	read_track # 调用 read_track 函数读取扇区。
	mov 	%ax, %cx
	add 	sread, %ax
	#seg cs
	cmp 	%cs:sectors+0, %ax
	jne 	ok3_read
	mov 	$1, %ax
	sub 	head, %ax
	jne 	ok4_read
	incw    track 

# ok4_read：
# 更新磁头和磁道编号，处理跨磁头 / 磁道的情况。
ok4_read:
	mov	%ax, head
	xor	%ax, %ax

# ok3_read：
# 1.更新磁头和已读扇区数。
# 2.调整内存地址（偏移量或段地址），确保数据连续存储。
ok3_read:
	mov	%ax, sread
	shl	$9, %cx
	add	%cx, %bx
	jnc	rp_read
	mov	%es, %ax
	add	$0x1000, %ax
	mov	%ax, %es
	xor	%bx, %bx
	jmp	rp_read

# read_track:
# 读取磁盘上数据
# 通过 BIOS 中断实现了：从指定磁道 / 扇区读取数据到内存，并提供出错处理机制

read_track:
    push    %ax              # 保存AX寄存器（函数调用约定）
    push    %bx              # 保存BX寄存器（数据缓冲区偏移量）
    push    %cx              # 保存CX寄存器（循环计数器/扇区参数）
    push    %dx              # 保存DX寄存器（驱动器/磁头参数）
    
    mov     track, %dx       # 加载当前磁道号到DX（低8位DL有效）
    mov     sread, %cx       # 加载当前扇区号到CX（从0开始计数）
    inc     %cx              # 扇区号+1（磁盘扇区编号从1开始）
    
    mov     %dl, %ch         # 将磁道号放入CH寄存器（高8位磁道号）
    mov     head, %dx        # 加载当前磁头号到DX（低8位DL有效）
    mov     %dl, %dh         # 将磁头号放入DH寄存器
    
    mov     $0, %dl          # DL = 0（驱动器号：0=第一个软盘）
    and     $0x0100, %dx     # 清除DX高位，仅保留DH中的磁头号
    
    mov     $2, %ah          # AH = 2（BIOS功能号：读扇区）
    int     $0x13            # 调用BIOS磁盘I/O中断
    
    jc      bad_rt           # 如果进位标志CF=1，表示错误，跳转到错误处理
    
    pop     %dx              # 恢复DX寄存器
    pop     %cx              # 恢复CX寄存器
    pop     %bx              # 恢复BX寄存器
    pop     %ax              # 恢复AX寄存器（AL包含实际读取的扇区数）
    ret                      # 返回调用者，AL为返回值

bad_rt:                 # 磁盘读取错误处理入口
    mov     $0, %ax      # AH=0（重置磁盘控制器功能号）
    mov     $0, %dx      # DL=0（驱动器号：0=第一个软盘）
    int     $0x13        # 调用BIOS重置磁盘控制器(0x13)中断是专门用于重置磁盘控制器的中断。
    
    pop     %dx          # 恢复DX寄存器（丢弃错误时保存的值）
    pop     %cx          # 恢复CX寄存器
    pop     %bx          # 恢复BX寄存器
    pop     %ax          # 恢复AX寄存器
    
    jmp     read_track   # 重试读取操作（跳转到函数开头）

#/*
# * This procedure turns off the floppy drive motor, so
# * that we enter the kernel in a known state, and
# * don't have to worry about it later.
# */

kill_motor:
	push	%dx
	mov	$0x3f2, %dx
	mov	$0, %al
	outsb
	pop	%dx
	ret

sectors:
	.word 0
# 这里是加载system期间的的字符，上面指定了字符的个数！
msg1:
	.byte 13,10
	# .ascii "IceCityOS is booting ..."
	.ascii "This is bootsect working..."
	.ascii "Loading system..."
	.byte 13,10,13,10
	.ascii "Welcome to Ethan Yankang's Linux..."
	.byte 13,10,13,10


# BIOS 加载规则：

# BIOS 会将磁盘的第一个扇区（0 柱面、0 磁头、1 扇区）加载到内存地址 0x7C00。
# 这个扇区的大小必须是 512 字节，且最后两个字节必须是 0x55AA（引导标志）。
# 如果不符合上述条件，BIOS 会认为这不是一个有效的引导扇区，从而报错。

	.org 508 # 告诉汇编器从地址 508处 开始放置后续代码和数据。
			 # 因为只有这样，后后面的2字节的根设备，2字节的引导标志就会刚刚对齐地址512b，这是BIOS加载的规则
root_dev:
	.word ROOT_DEV # 根设备号（2字节）
boot_flag:
	.word 0xAA55 # 引导标志0x55AA(2字节)，用于标识这是一个有效的引导扇区。
	
	.text       # 切换到代码段
	endtext:    # 标记代码段的结束位置
	.data       # 切换到数据段
	enddata:    # 标记数据段的结束位置
	.bss        # 切换到BSS段
	endbss:     # 标记BSS段的结束位置