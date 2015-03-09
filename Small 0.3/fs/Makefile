#
# Small/fs/Makefile
#
# (C) 2012-2013 Yafei Zheng
# V0.3 2013-02-23 11:31:15
#
# Email: e9999e@163.com, QQ: 1039332004
#

# --------------------------------------------
# 变量定义

AS		= 	as
LD		=	ld
ASEFLAGES	=
LDEFLAGES	=	-Ttext 0

CC		=	gcc
# c99标准支持在for循环初始化语句中定义变量
# -fno-builtin 选项，关闭GCC的内置函数替换功能，比如禁止将 printf("abc\n") 替换为 puts("abc")
# NOTE! 选项： -fomit-frame-pointer --- ERROR: can't find a register in class 'GENERAL_REGS' while reloading 'asm'.
CEFLAGES		=	-Wall -O0 -std=c99 -I../include -nostdinc -fno-builtin -fomit-frame-pointer
# --------------------------------------------

# --------------------------------------------
all: fs.o

OBJS = buffer.o inode.o alloc.o read_write.o

fs.o: $(OBJS)
	$(LD) -r -o $@ $(OBJS)

buffer.o: buffer.c
	$(CC) $(CEFLAGES) -c $<

inode.o: inode.c
	$(CC) $(CEFLAGES) -c $<

alloc.o: alloc.c
	$(CC) $(CEFLAGES) -c $<

read_write.o: read_write.c
	$(CC) $(CEFLAGES) -c $<
# --------------------------------------------

# --------------------------------------------
# 清除
.PHONY: clean
clean:
	-rm *.a *.o
# --------------------------------------------