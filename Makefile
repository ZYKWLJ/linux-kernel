OS = Mac

# indicate the Hardware Image file（指示硬盘镜像文件）
HDA_IMG = hdc-0.11.img

# indicate the path of the calltree（指示调用树路径）
# 通过 find 命令查找名为 calltree 的可执行文件（用于生成函数调用图）
CALLTREE=$(shell find tools/ -name "calltree" -perm 755 -type f)

# indicate the path of the bochs（指示bocks的路径）
#BOCHS=$(shell find tools/ -name "bochs" -perm 755 -type f)
# 用途：用于启动 Bochs 模拟器进行内核调试。
BOCHS=bochs

#
# if you want the ram-disk device, define this to be the
# size in blocks.
#
# 这里指明是不是要用虚拟磁盘，是物理内存划分区域的第三块，之前为此还进行了专门的讨论！
RAMDISK =  #-DRAMDISK=512，虚拟磁盘大小为512块，也就是512KB，这里注释了，即未指定！

# This is a basic Makefile for setting the general configuration
# 包含同目录下的 Makefile.header，引入公共编译配置（如编译器、头文件路径、优化选项等）。
# Makefile 的 include 指令会将目标文件内容插入当前位置，实现`配置复用`（类似 C 语言的 `#include`）
include Makefile.header 

# 这一句是链接器参数,确保链接后的`内核二进制`文件`符合内存布局要求`。：
# -Ttext 0：设置代码段起始地址为`内存地址 0`（适配 32 位启动）。
# -e startup_32：指定程序入口为 `startup_32` 函数。

LDFLAGS    += -Ttext 0 -e startup_32

# 编译器参数（CFLAGS 和 CPP）
# $(RAMDISK)：若启用虚拟磁盘，此处会传入 -DRAMDISK=512 等宏定义。 
# -Iinclude：添加 include/ 目录到头文件搜索路径，使编译器能找到 *.h 文件。
CFLAGS    += $(RAMDISK) -Iinclude

# -Iinclude 为预处理器（CPP）指定头文件路径，与 CFLAGS 作用相同（重复配置，可能为历史原因）。
CPP    += -Iinclude

#
# ROOT_DEV specifies the default root-device when making the image.
# This can be either FLOPPY, /dev/xxxx or empty, in which case the
# default of /dev/hd6 is used by 'build'.
#
# 作用：指定内核镜像的默认根设备。
# FLOPPY：根设备为软盘（适用于从软盘启动）。 
# 空值：默认使用 /dev/hd6（虚拟硬盘的`第 6 个分区`，对应 hda1 在 Linux 0.11 中的编号）。
# 当前配置：未启用软盘（被注释掉了），使用默认硬盘分区。
ROOT_DEV= #FLOPPY 

# 定义模块文件列表
# 核心功能模块的目标文件（如进程调度、内存管理、文件系统）。
ARCHIVES=kernel/kernel.o mm/mm.o fs/fs.o
# 设备驱动库文件（块设备、字符设备驱动）。
DRIVERS =kernel/blk_drv/blk_drv.a kernel/chr_drv/chr_drv.a
# 数学运算库（可能包含浮点模拟代码）。
MATH    =kernel/math/math.a
# 标准库文件（如 lib/ 中的字符串处理、I/O 函数）。
LIBS    =lib/lib.a

# 模式匹配的编译规则！

# .c.s：将 C 源文件编译为汇编代码（-S 选项，生成 .s 文件）。
# 符号说明：
# $<：依赖项中的第一个文件（如 file.c）。
# $*.s：目标文件名（如 file.s），* 表示通配符。

# .c.s  .s.o .c.o  为模式匹配规则，可以匹配多个文件！
# 使用模式匹配，使得make命令新增了基于目标输出文件的编译规则。可以使得make hello.s有效，
# 因为make会自动查找与hello.s匹配的规则，即.c.s规则。
# 同时，使得make foo.s bar.s有效，因为make会自动查找与foo.s和bar.s匹配的规则，即.c.s规则。

.c.s:
	@$(CC) $(CFLAGS) -S -o $*.s $<

# .s.o：将汇编文件编译为目标文件（.o，使用汇编器 $(AS)）。
.s.o:
	@$(AS)  -o $*.o $<

# .c.o：直接将 C 源文件编译为目标文件（-c 选项，不链接）。
.c.o:
	@$(CC) $(CFLAGS) -c -o $*.o $<

# 详细模式规则的实例，可以看公众号：


# 主目标(all)：生成内核镜像（Image）
# all 目标：默认编译目标，依赖 Image。
# Image 依赖：
# boot/bootsect：引导扇区代码（负责加载内核到内存）。
# boot/setup：初始化设置代码（读取硬件信息）。
# tools/system：内核主体二进制文件。

all:    Image    

Image: boot/bootsect boot/setup tools/system
    # 复制 tools/system 为临时文件 system.tmp。
	@cp -f tools/system system.tmp
    # 使用 strip 去除调试符号，减小文件体积。
	@$(STRIP) system.tmp
    # 使用 objcopy 提取二进制内容，去除无关段（.note, .comment）。
	@$(OBJCOPY) -O binary -R .note -R .comment system.tmp tools/kernel
    # 运行 tools/build.sh 脚本，将引导程序、设置程序和内核二进制合并为可启动镜像 Image。
	@tools/build.sh boot/bootsect boot/setup tools/kernel Image $(ROOT_DEV)
    # 清理临时文件，同步磁盘缓存。
	@rm system.tmp
	@rm -f tools/kernel
    # 可以将 sync 理解为 “保存并关闭文档” 的操作：在编辑器中修改文件后，点击 “保存” 只是将数据写入内存缓存，而 “关闭文档” 前的强制保存（类似 sync）才会真正将数据写入硬盘。在 Makefile 中，sync 是确保编译产物完整、可靠的最后一道防线，尤其在涉及文件删除或系统重启等操作前至关重要。
    # @sync 的作用是强制将操作系统缓存中的数据立即写入物理磁盘，确保数据的完整性和一致性。
	@sync

# 作用：将内核镜像 Image 写入软盘设备（/dev/fd0），用于物理软盘启动。

# dd 命令：以 8KB 块大小复制文件到设备。
# bs=8192：指定块大小为 8KB。
# if=Image：指定输入文件为 Image（内核镜像）。
# of=/dev/fd0：指定输出设备为 /dev/fd0（软盘设备）。
# 注意：这是一个物理操作，会直接写入软盘，确保数据安全。

disk: Image
	@dd bs=8192 if=Image of=/dev/fd0

# 通过嵌套 Makefile，在 boot/ 目录下编译 head.s 生成 head.o，并将结果返回给上层构建系统。
# 这是一个典型的 Makefile 嵌套结构，用于模块化构建和管理复杂的项目结构。
# 作用：编译 boot/head.s 生成 boot/head.o，用于构建内核镜像。
boot/head.o: boot/head.s
	@make head.o -C boot/

# 作用：链接所有核心模块，生成内核主体二进制 tools/system。
# nm 命令：提取符号表，过滤无关符号后生成 System.map（用于调试时定位函数地址）。
tools/system:    boot/head.o init/main.o \
        $(ARCHIVES) $(DRIVERS) $(MATH) $(LIBS)
	@$(LD) $(LDFLAGS) boot/head.o init/main.o \
	$(ARCHIVES) \
	$(DRIVERS) \
	$(MATH) \
	$(LIBS) \
	-o tools/system 
	@nm tools/system | grep -v '\(compiled\)\|\(\.o$$\)\|\( [aU] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)'| sort > System.map 

# 作用：通过递归编译子目录，实现模块化构建。

kernel/math/math.a:
	@make -C kernel/math

kernel/blk_drv/blk_drv.a:
	@make -C kernel/blk_drv

kernel/chr_drv/chr_drv.a:
	@make -C kernel/chr_drv

kernel/kernel.o:
	@make -C kernel

mm/mm.o:
	@make -C mm

fs/fs.o:
	@make -C fs

lib/lib.a:
	@make -C lib

boot/setup: boot/setup.s
	@make setup -C boot

boot/bootsect: boot/bootsect.s
	@make bootsect -C boot

tmp.s:    boot/bootsect.s tools/system
	@(echo -n "SYSSIZE = (";ls -l tools/system | grep system \
        | cut -c25-31 | tr '\012' ' '; echo "+ 15 ) / 16") > tmp.s
	@cat boot/bootsect.s >> tmp.s

# clean：清理中间文件
clean:
	@rm -f Image System.map tmp_make core boot/bootsect boot/setup
	@rm -f init/*.o tools/system boot/*.o typescript* info bochsout.txt
	@for i in mm fs kernel lib boot; do make clean -C $$i; done 
info:
	@make clean
	@script -q -c "make all"
	@cat typescript | col -bp | grep -E "warning|Error" > info
	@cat info
# distclean：彻底清理
distclean: clean
	@rm -f tag cscope* linux-0.11.* $(CALLTREE)
	@(find tools/calltree-2.3 -name "*.o" | xargs -i rm -f {})
	@make clean -C tools/calltree-2.3
	@make clean -C tools/bochs/bochs-2.3.7

backup: clean
	@(cd .. ; tar cf - linux | compress16 - > backup.Z)
	@sync

dep:
	@sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	@(for i in init/*.c;do echo -n "init/";$(CPP) -M $$i;done) >> tmp_make
	@cp tmp_make Makefile
	@for i in fs kernel mm; do make dep -C $$i; done

# cscope：生成代码索引数据库，用于快速查找符号定义和引用。
# ctags：生成 tags 文件，供 Vim 等编辑器实现跳转功能。
tag: tags
tags:
	@ctags -R

cscope:
	@cscope -Rbkq

start:
	@qemu-system-x86_64 -m 16M -boot a -fda Image -hda $(HDA_IMG)

debug:
	@echo $(OS)
	@qemu-system-x86_64 -m 16M -boot a -fda Image -hda $(HDA_IMG) -s -S

bochs-debug:
	@$(BOCHS) -q -f tools/bochs/bochsrc/bochsrc-hd-dbg.bxrc    

bochs:
ifeq ($(BOCHS),)
	@(cd tools/bochs/bochs-2.3.7; \
    ./configure --enable-plugins --enable-disasm --enable-gdb-stub;\
	make)
endif

bochs-clean:
	@make clean -C tools/bochs/bochs-2.3.7

calltree:
ifeq ($(CALLTREE),)
	@make -C tools/calltree-2.3
endif

calltree-clean:
	@(find tools/calltree-2.3 -name "*.o" \
	-o -name "calltree" -type f | xargs -i rm -f {})

# cg：生成函数调用图
# 作用：使用 `calltree` 工具分析 init/main.c 的函数调用关系，生成 DOT 格式图，再转换为 JPG 图片。

cg: callgraph
callgraph:
	@calltree -b -np -m init/main.c | tools/tree2dotx > linux-0.11.dot
	@dot -Tjpg linux-0.11.dot -o linux-0.11.jpg

# 作用：打印用户指南，说明常用编译目标（如 make start、make clean）和依赖工具。
help:
	@echo "<<<<This is the basic help info of linux-0.11>>>"
	@echo ""
	@echo "Usage:"
	@echo "     make --generate a kernel floppy Image with a fs on hda1"
	@echo "     make start -- start the kernel in qemu"
	@echo "     make debug -- debug the kernel in qemu & gdb at port 1234"
	@echo "     make disk  -- generate a kernel Image & copy it to floppy"
	@echo "     make cscope -- genereate the cscope index databases"
	@echo "     make tags -- generate the tag file"
	@echo "     make cg -- generate callgraph of the system architecture"
	@echo "     make clean -- clean the object files"
	@echo "     make distclean -- only keep the source code files"
	@echo ""
	@echo "Note!:"
	@echo "     * You need to install the following basic tools:"
	@echo "          ubuntu|debian, qemu|bochs, ctags, cscope, calltree, graphviz "
	@echo "          vim-full, build-essential, hex, dd, gcc 4.3.2..."
	@echo "     * Becarefull to change the compiling options, which will heavily"
	@echo "     influence the compiling procedure and running result."
	@echo ""
	@echo "Author:"
	@echo "     * 1991, linus write and release the original linux 0.95(linux 0.11)."
	@echo "     * 2005, jiong.zhao<gohigh@sh163.net> release a new version "
	@echo "     which can be used in RedHat 9 along with the book 'Explaining "
	@echo "     Linux-0.11 Completly', and he build a site http://www.oldlinux.org"
	@echo "     * 2008, falcon<wuzhangjin@gmail.com> release a new version which can be"
	@echo "     used in ubuntu|debian 32bit|64bit with gcc 4.3.2, and give some new "
	@echo "     features for experimenting. such as this help info, boot/bootsect.s and"
	@echo "     boot/setup.s with AT&T rewritting, porting to gcc 4.3.2 :-)"
	@echo ""
	@echo "<<<Be Happy To Play With It :-)>>>"


# 作用：显式声明 init/main.o 的依赖头文件，确保头文件变更时自动重新编译。
# 生成方式：通过 make dep 目标自动扫描生成，避免手动维护依赖关系。
# 声明 init/main.o 的编译依赖于 init/main.c 源文件及 20 个相关头文件，确保任何依赖文件变化时都能触发重新编译。

### Dependencies:
init/main.o: init/main.c include/unistd.h include/sys/stat.h \
  include/sys/types.h include/sys/times.h include/sys/utsname.h \
  include/utime.h include/time.h include/linux/tty.h include/termios.h \
  include/linux/sched.h include/linux/head.h include/linux/fs.h \
  include/linux/mm.h include/signal.h include/asm/system.h \
  include/asm/io.h include/stddef.h include/stdarg.h include/fcntl.h
