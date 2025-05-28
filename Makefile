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

.c.s:
    @$(CC) $(CFLAGS) -S -o $*.s $<

# .s.o：将汇编文件编译为目标文件（.o，使用汇编器 $(AS)）。
.s.o:
    @$(AS)  -o $*.o $<

# .c.o：直接将 C 源文件编译为目标文件（-c 选项，不链接）。
.c.o:
    @$(CC) $(CFLAGS) -c -o $*.o $<




all:    Image    

Image: boot/bootsect boot/setup tools/system
    @cp -f tools/system system.tmp
    @$(STRIP) system.tmp
    @$(OBJCOPY) -O binary -R .note -R .comment system.tmp tools/kernel
    @tools/build.sh boot/bootsect boot/setup tools/kernel Image $(ROOT_DEV)
    @rm system.tmp
    @rm -f tools/kernel
    @sync

disk: Image
    @dd bs=8192 if=Image of=/dev/fd0

boot/head.o: boot/head.s
    @make head.o -C boot/

tools/system:    boot/head.o init/main.o \
        $(ARCHIVES) $(DRIVERS) $(MATH) $(LIBS)
    @$(LD) $(LDFLAGS) boot/head.o init/main.o \
    $(ARCHIVES) \
    $(DRIVERS) \
    $(MATH) \
    $(LIBS) \
    -o tools/system 
    @nm tools/system | grep -v '\(compiled\)\|\(\.o$$\)\|\( [aU] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)'| sort > System.map 

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

clean:
    @rm -f Image System.map tmp_make core boot/bootsect boot/setup
    @rm -f init/*.o tools/system boot/*.o typescript* info bochsout.txt
    @for i in mm fs kernel lib boot; do make clean -C $$i; done 
info:
    @make clean
    @script -q -c "make all"
    @cat typescript | col -bp | grep -E "warning|Error" > info
    @cat info

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

cg: callgraph
callgraph:
    @calltree -b -np -m init/main.c | tools/tree2dotx > linux-0.11.dot
    @dot -Tjpg linux-0.11.dot -o linux-0.11.jpg

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

### Dependencies:
init/main.o: init/main.c include/unistd.h include/sys/stat.h \
  include/sys/types.h include/sys/times.h include/sys/utsname.h \
  include/utime.h include/time.h include/linux/tty.h include/termios.h \
  include/linux/sched.h include/linux/head.h include/linux/fs.h \
  include/linux/mm.h include/signal.h include/asm/system.h \
  include/asm/io.h include/stddef.h include/stdarg.h include/fcntl.h
