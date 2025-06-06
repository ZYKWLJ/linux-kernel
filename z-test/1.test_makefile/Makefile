# Makefile
CC = gcc
CFLAGS = -Wall -O2 -Iinclude
AT= @


# 模式规则
.c.s:
	$(AT)$(CC) $(CFLAGS) -S -o $*.s $<

# .s.o：将汇编文件编译为目标文件（.o，使用汇编器 $(AS)）。
.s.o:
	$(AT)$(AS)  -o $*.o $<

# .c.o：直接将 C 源文件编译为目标文件（-c 选项，不链接）。
.c.o:
	$(AT)$(CC) $(CFLAGS) -c -o $*.o $<
	
# 这也是第一个目标，也是默认目标，输入make命令时，会执行这个目标
# 最终目标：生成可执行文件
hello: hello.o
	$(AT)$(CC) $(CFLAGS) -o hello hello.o

# 清理目标
clean:
	$(AT)rm -f *.o *.s hello