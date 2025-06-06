	.code16
# rewrite with AT&T syntax by falcon <wuzhangjin@gmail.com> at 081012
#
#	setup.s		(C) 1991 Linus Torvalds
#
# setup.s is responsible for getting the system data from the BIOS,
# and putting them into the appropriate places in system memory.
# both setup.s and system has been loaded by the bootblock.
#
# This code asks the bios for memory/disk/other parameters, and
# puts them in a "safe" place: 0x90000-0x901FF, ie where the
# boot-block used to be. It is then up to the protected mode
# system to read them from there before the area is overwritten
# for buffer-blocks.
#

# NOTE! These had better be the same as in bootsect.s!
# 这里最好和前面的bootsect.s中的相等，为了一致性考虑

	.equ INITSEG, 0x9000	# we move boot here - out of the way，这是目前的bootsect的位置
	.equ SYSSEG, 0x1000	# system loaded at 0x10000 (65536).这是目前的system的位置
	.equ SETUPSEG, 0x9020	# this is the current segment.这是目前的setup模块位置

	.global _start, begtext, begdata, begbss, endtext, enddata, endbss
	.text
	begtext:
	.data
	begdata:
	.bss
	begbss:
	.text

# 长跳转指令，用于跨段跳转（修改 CS:IP 寄存器）
# 效果：跳转到0x90200 + _start偏移量处执行，同时刷新 CPU 的代码段缓存

	ljmp $SETUPSEG, $_start	 

# 程序的起始地址

_start:
	mov %cs,%ax
	mov %ax,%ds
	mov %ax,%es
#
##print some message
#
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10

	mov $29, %cx
	mov $0x000b,%bx
	mov $msg2,%bp
	mov $0x1301, %ax
	int $0x10

####################################################################################
#                    setup.s的第一个功能模块==>读取机器的参数                         #
####################################################################################

# ok, the read went well so we get current cursor position and save it for
# posterity.
	
    mov	$INITSEG, %ax	# this is done in bootsect already, but...

# 这里再次将ds设置成0x9000，虽然在bootsect中设置过了，但是，为了模块分明，linus觉得应该再次设置一下。

	mov	%ax, %ds
	mov	$0x03, %ah	# read cursor pos
	xor	%bh, %bh

# 获取光标信息----将光标位置存放在0x90000处，控制台初始化时会来取。

	int	$0x10		# save it in known place, con_init fetches
	mov	%dx, %ds:0	# it from 0x90000.

# Get memory size (extended mem, kB)
# 获取扩拓展内存的大小(KB)信息----并将其存放在0x90002处。
	mov	$0x88, %ah # BIOS的功能号0x88
	int	$0x15 # 调用BIOS中断0x15,将返回的扩展内存大小放在%ax里面
	mov	%ax, %ds:2 # 将ax的值存放在0x90002处

# 这里将来可以做一个接口，提供后面的打印！ 
info_extended_memory:
	.byte 13,10
	.ascii "Now we are in setup ..."
	.byte 13,10,13,10

# Get video-card data:
# 获取显示卡当前的显示模式----存放在在0x90004、0x90006中
	mov	$0x0f, %ah
	int	$0x10
	mov	%bx, %ds:4	# bh = display page 存放当前页面
	mov	%ax, %ds:6	# al = video mode, ah = window width 存放显示模式
# %ds:7 存放字符号列数

# check for EGA/VGA and some config parameters
# 获取显示方式(EGA/VGA)并取参数----存放在在0x90008、0x9000A、0x9000C中
	mov	$0x12, %ah
	mov	$0x10, %bl
	int	$0x10
	mov	%ax, %ds:8 # 存放当前功能号？
	mov	%bx, %ds:10 # 存放安装的显示内存+显示状态(彩色/单色)
	mov	%cx, %ds:12 # 显示卡特性参数

# Get hd0 data
# 取第一个磁盘信息----存放在在`0x90080`中
	mov	$0x0000, %ax
	mov	%ax, %ds
	lds	%ds:4*0x41, %si # 取中段向量0x41的值，即hd0参数表的地址
	mov	$INITSEG, %ax
	mov	%ax, %es
	mov	$0x0080, %di # 存放hd0参数表的地址为：0x90080
	mov	$0x10, %cx # 共传输0x10字节(16字节，这也是表的长度)
	rep
	movsb

# Get hd1 data
# 取第二个硬盘信息----存放在`0x90090`中(理所当然，紧紧贴着硬盘1存放的位置)
	mov	$0x0000, %ax
	mov	%ax, %ds
	lds	%ds:4*0x42, %si
	mov	$0x0000, %ax
	mov	%ax, %ds
	lds	%ds:4*0x46, %si # 取中断向量0x46的值，也即hd1参数表的地址
	mov	$INITSEG, %ax
	mov	%ax, %es
	mov	$0x0090, %di # 存放hd1参数表的地址为：0x90090
	mov	$0x10, %cx # 共传输0x10字节(16字节，这也是表的长度)
	rep
	movsb
####################################################################
#这中间一部分都是后面再加上去的方便调试的信息，不是linus的原版本的信息。



## modify ds
	mov $INITSEG,%ax
	mov %ax,%ds
	mov $SETUPSEG,%ax
	mov %ax,%es

##show cursor pos:
	mov $0x03, %ah 
	xor %bh,%bh
	int $0x10
	mov $11,%cx
	mov $0x000c,%bx
	mov $cur,%bp
	mov $0x1301,%ax
	int $0x10
##show detail
	mov %ds:0 ,%ax
	call print_hex
	call print_nl

##show memory size
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10
	mov $12, %cx
	mov $0x000a, %bx
	mov $mem, %bp
	mov $0x1301, %ax
	int $0x10

##show detail
	mov %ds:2 , %ax
	call print_hex

##show 
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10
	mov $25, %cx
	mov $0x000d, %bx
	mov $cyl, %bp
	mov $0x1301, %ax
	int $0x10
##show detail
	mov %ds:0x80, %ax
	call print_hex
	call print_nl

##show 
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10
	mov $8, %cx
	mov $0x000e, %bx
	mov $head, %bp
	mov $0x1301, %ax
	int $0x10
##show detail
	mov %ds:0x82, %ax
	call print_hex
	call print_nl

##show 
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10
	mov $8, %cx
	mov $0x000f, %bx
	mov $sect, %bp
	mov $0x1301, %ax
	int $0x10
##show detail
	mov %ds:0x8e, %ax
	call print_hex
	call print_nl
#l:
#	jmp l
##

# 这中间一部分都是后面再加上去的方便调试的信息，不是linus的原版本的信息。
####################################################################



# 检查系统是否有第2个磁盘，如果不存在就把2个表清空，利用BIOS中断调用0x13的取盘功能。
# Check that there IS a hd1 :-)

	mov	$0x01500, %ax
	mov	$0x81, %dl
	int	$0x13
	jc	no_disk1 
	cmp	$3, %ah # 是硬盘吗(硬件类型=3)
	je	is_disk1 # 如果两者相等的话，就是，跳转到is_disk上去

no_disk1:
	mov	$INITSEG, %ax # 第二个硬盘不存在就对他的硬盘表清零。
	mov	%ax, %es
	mov	$0x0090, %di
	mov	$0x10, %cx
	mov	$0x00, %ax
	rep
	stosb

is_disk1:



# now we want to move to protected mode ...
# 现在我们想移动到保护模式中去

# 移动到保护模式过程中不允许中断的！
	cli			# no interrupts allowed ! 

####################################################################################
#                    setup.s的第2个功能模块==>读取system模块到0x00000处               #
####################################################################################

# first we move the system to it's rightful place

	mov	$0x0000, %ax #移动到的目的地址
	cld			# 'direction'=0, movs moves forward

do_move:
	mov	%ax, %es	# destination segment 目的代码
	add	$0x1000, %ax
	cmp	$0x9000, %ax #判断是不是已经把0x8000段开始的64KB的代码移动完了？
	jz	end_move
	mov	%ax, %ds	# source segment 源地址
	sub	%di, %di
	sub	%si, %si
	mov 	$0x8000, %cx #移动0x80000字节，也即64KB
	rep # 没有移动完，这里会一直移动！
	movsw
	jmp	do_move


####################################################################################
#                setup.s的第3个功能模块==>为head.s工作在保护模式下做一些系统初始化工作  #
####################################################################################

# system移动完成后我们开始加载段描述符了
# then we load the segment descriptors

end_move: # 所以这个过程的名字叫结束移动(end_move)
	mov	$SETUPSEG, %ax	# right, forgot this at first. didn't work :-)
	mov	%ax, %ds #ds指向本程序(setup)段

####################################################################################
#                setup.s的第3个功能模块1==>临时设置idt、gdt                          #
####################################################################################    
# 这里的ligt和lgdt是intelCPU实现的加载段描述符表寄存器的指令！
	lidt	idt_48		# load idt with 0,0 #加载中断描述符表寄存器
	lgdt	gdt_48		# load gdt with whatever appropriate #加载全局描述符表寄存器

# 使能A20地址线，历史遗留问题(为了考虑兼容性而加的)
# that was painless, now we enable A20

	#call	empty_8042	# 8042 is the keyboard controller
	#mov	$0xD1, %al	# command write
	#out	%al, $0x64
	#call	empty_8042
	#mov	$0xDF, %al	# A20 on
	#out	%al, $0x60
	#call	empty_8042
	inb     $0x92, %al	# open A20 line(Fast Gate A20).
	orb     $0b00000010, %al
	outb    %al, $0x92


# 这里是设置8259芯片中断的模块，将0x20-0x2F设置为其中断号
# 本质上来说，是CPU与该芯片交互，设置中断。

####################################################################################
#             setup.s的第3个功能模块2==>摄制部8259芯片中断号0x20-0x2F                 #
#################################################################################### 

# well, that went ok, I hope. Now we have to reprogram the interrupts :-(
# we put them right after the intel-reserved hardware interrupts, at
# int 0x20-0x2F. There they won't mess up anything. Sadly IBM really
# messed this up with the original PC, and they haven't been able to
# rectify it afterwards. Thus the bios puts interrupts at 0x08-0x0f,
# which is used for the internal hardware interrupts as well. We just
# have to reprogram the 8259's, and it isn't fun.

	mov	$0x11, %al		# initialization sequence(ICW1)
					# ICW4 needed(1),CASCADE mode,Level-triggered
	out	%al, $0x20		# send it to 8259A-1
	.word	0x00eb,0x00eb		# jmp $+2, jmp $+2
	out	%al, $0xA0		# and to 8259A-2
	.word	0x00eb,0x00eb
	mov	$0x20, %al		# start of hardware int's (0x20)(ICW2)
	out	%al, $0x21		# from 0x20-0x27
	.word	0x00eb,0x00eb
	mov	$0x28, %al		# start of hardware int's 2 (0x28)
	out	%al, $0xA1		# from 0x28-0x2F
	.word	0x00eb,0x00eb		#               IR 7654 3210
	mov	$0x04, %al		# 8259-1 is master(0000 0100) --\
	out	%al, $0x21		#				|
	.word	0x00eb,0x00eb		#			 INT	/
	mov	$0x02, %al		# 8259-2 is slave(       010 --> 2)
	out	%al, $0xA1
	.word	0x00eb,0x00eb
	mov	$0x01, %al		# 8086 mode for both
	out	%al, $0x21
	.word	0x00eb,0x00eb
	out	%al, $0xA1
	.word	0x00eb,0x00eb
	mov	$0xFF, %al		# mask off all interrupts for now
	out	%al, $0x21
	.word	0x00eb,0x00eb
	out	%al, $0xA1

# 从此之后，就不再需要BIOS的工作了，接下来进行保护模式的切换了！


####################################################################################
#            setup.s的第3个功能模块3==>设置 CPU 的控制寄存器CR0，开启保护模式          #
#################################################################################### 

# well, that certainly wasn't fun :-(. Hopefully it works, and we don't
# need no steenking BIOS anyway (except for the initial loading :-).
# The BIOS-routine wants lots of unnecessary data, and it's less
# "interesting" anyway. This is how REAL programmers do it.
#
# Well, now's the time to actually move into protected mode. To make
# things as simple as possible, we do no register set-up or anything,
# we let the gnu-compiled 32-bit programs do that. We just jump to
# absolute address 0x00000, in 32-bit protected mode.

	#mov	$0x0001, %ax	# protected mode (PE) bit
	#lmsw	%ax		# This is it! # 加载机器状态字CR0(控制寄存器)
	mov	%cr0, %eax	# get machine status(cr0|MSW) #获取机器状态
	bts	$0, %eax	# turn on the PE-bit  
	mov	%eax, %cr0	# protection enabled          #开启保护模式
				
				# segment-descriptor        (INDEX:TI:RPL)
	.equ	sel_cs0, 0x0008 # select for code segment 0 (  001:0 :00) 

####################################################################################
#                   setup.s的第4个功能模块== 跳转到head.s模块执行                    #
####################################################################################

# 也就是下面这条指令，很重要的！直接跳转到head.s,同时也是setup阶段的最后一条指令。
	ljmp	$sel_cs0, $0	# jmp offset 0 of code segment 0 in gdt

# This routine checks that the keyboard command queue is empty
# No timeout is used - if this hangs there is something wrong with
# the machine, and we probably couldn't proceed anyway.

empty_8042:
	.word	0x00eb,0x00eb
	in	$0x64, %al	# 8042 status port
	test	$2, %al		# is input buffer full?
	jnz	empty_8042	# yes - loop
	ret

gdt:
	.word	0,0,0,0		# dummy

	.word	0x07FF		# 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		# base address=0
	.word	0x9A00		# code read/exec
	.word	0x00C0		# granularity=4096, 386

	.word	0x07FF		# 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		# base address=0
	.word	0x9200		# data read/write
	.word	0x00C0		# granularity=4096, 386

idt_48:
	.word	0			# idt limit=0
	.word	0,0			# idt base=0L

gdt_48:
	.word	0x800			# gdt limit=2048, 256 GDT entries
	.word   512+gdt, 0x9		# gdt base = 0X9xxxx, 
	# 512+gdt is the real gdt after setup is moved to 0x9020 * 0x10
print_hex:
	mov $4,%cx
	mov %ax,%dx

print_digit:
	rol $4,%dx	#循环以使低4位用上，高4位移至低4位
	mov $0xe0f,%ax #ah ＝ 请求的功能值，al = 半个字节的掩码
	and %dl,%al
	add $0x30,%al
	cmp $0x3a,%al
	jl outp
	add $0x07,%al

outp:
	int $0x10
	loop print_digit
	ret
#打印回车换行
print_nl:
	mov $0xe0d,%ax
	int $0x10
	mov $0xa,%al
	int $0x10
	ret

msg2:
	.byte 13,10
	.ascii "Now we are in setup ..."
	.byte 13,10,13,10
cur:
	.ascii "Cursor POS:"
mem:
	.ascii "Memory SIZE:"
cyl:
	.ascii "KB"
	.byte 13,10,13,10
	.ascii "HD Info"
	.byte 13,10
	.ascii "Cylinders:"
head:
	.ascii "Headers:"
sect:
	.ascii "Secotrs:"
.text
endtext:
.data
enddata:
.bss
endbss:
