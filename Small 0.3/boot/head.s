#
# Small/boot/head.s
#
# (C) 2012-2013 Yafei Zheng
# V0.0 2012-12-7 10:44:39
# V0.1 2013-01-30 02:40:57
# V0.2 2013-02-01 15:35:26
# V0.3 2013-02-20 17:45:09
#
# Email: e9999e@163.com, QQ: 1039332004
#

# **************************************************************************************************
# 编译器：GNU as 2.21.1
#
# head.s的主要工作如下：
#	1.重新设置GDT，IDT，内核堆栈（任务0用户态堆栈）
#	2.设置默认中断
#	3.设置页表，开启分页模式
# **************************************************************************************************

BOOTPOS		= 0x90000			# 启动扇区被移动到的位置。即 INITSEG * 16
SCRN_SEL	= 0x18				# 屏幕显示内存段选择符
KERNEL_PG_DIR_START	= 768		# 内核的页目录的开始项。NOTE! 内核占用的虚拟地址空间为：3GB - 4GB.

.text

.globl startup_32
.globl pg_dir, idt, gdt, init_stack

pg_dir:							# 进程0的页目录表
startup_32:
	movl	$0x10,%eax
	mov		%ax,%ds
	mov		%ax,%es
	mov		%ax,%fs
	mov		%ax,%gs
	lss		init_stack,%esp
	jmp		after_page

# ----------------------------------------------------

# 以下是进程0的页表。目前只支持16MB内存，故页表只有4页。
.org 0x1000
pg0:

.org 0x2000
pg1:

.org 0x3000
pg2:

.org 0x4000
pg3:

# ----------------------------------------------------

.org 0x5000
after_page:
# 设置进程0的页目录和页表，并开启分页模式。
# NOTE! 打开分页模式之后，由于内核虚拟空间为3GB-4GB，而目前的段基址还是0x00，
# 故对GDT表的重新设置要马上进行! 因为此处还使用着非规格的页目录（从0项开始的页目录）。
	call	setup_page
# 在新的位置重新设置GDT,IDT
	call	setup_gdt
	call	setup_idt
	movl	$0x10,%eax
	mov		%ax,%ds
	mov		%ax,%es
	mov		%ax,%fs
	mov		%ax,%gs
	lss		init_stack,%esp

# 对8259A重编程，设置其中断号为int 0x20-0x2F。此代码参照linux-0.11
	mov		$0x11,%al		# initi%alization sequence
	out		%al,$0x20		# send it to 8259A-1
	.word	0x00eb,0x00eb	# jmp $+2, jmp $+2
	out		%al,$0xA0		# and to 8259A-2
	.word	0x00eb,0x00eb
	mov		$0x20,%al		# start of hardware int's (0x20)
	out		%al,$0x21
	.word	0x00eb,0x00eb
	mov		$0x28,%al		# start of hardware int's 2 (0x28)
	out		%al,$0xA1
	.word	0x00eb,0x00eb
	mov		$0x04,%al		# 8259-1 is master
	out		%al,$0x21
	.word	0x00eb,0x00eb
	mov		$0x02,%al		# 8259-2 is slave
	out		%al,$0xA1
	.word	0x00eb,0x00eb
	mov		$0x01,%al		# 8086 mode for both
	out		%al,$0x21
	.word	0x00eb,0x00eb
	out		%al,$0xA1
	.word	0x00eb,0x00eb
	# 将以下语句注释，此处不屏蔽中断
#	mov		$0xFF,%al		# mask off %all interrupts for now
#	out		%al,$0x21
#	.word	0x00eb,0x00eb
#	out		%al,$0xA1

# OK! We begin to run the MAIN function now.
	pushl	$main
	ret						# 进入main函数执行

# ----------------------------------------------------

#
# 设置进程0的页目录项和页表项，并开启分页模式。NOTE! 内核默认的内存大小为16MB。
#
# NOTE! 进程0页目录项包括：用户态页目录表项、内核态页目录表项。
#
.align 4
setup_page:
	# 循环清零
	movl	$1024*5,%ecx							# 5 pages - pg_dir+4 page tables
	xorl	%eax,%eax
	xorl	%edi,%edi								# pg_dir is at 0x000
	cld; rep; stosl
	# 设置用户态页目录表项，U/S - 1。NOTE! 在重新设置GDT之前，需要这些页目录项
	movl	$0*4+pg_dir,%eax						# 用户态的页目录项地址
	movl	$pg0+7,0(%eax)							# U/S - 1, R/W - 1, P - 1
	movl	$pg1+7,4(%eax)
	movl	$pg2+7,8(%eax)
	movl	$pg3+7,12(%eax)
	# 设置内核态页目录表项，U/S - 0。内核虚拟空间在3GB-4GB.
	movl	$KERNEL_PG_DIR_START*4+pg_dir,%eax		# 内核态的页目录项地址
	movl	$pg0+3,0(%eax)							# U/S - 0, R/W - 1, P - 1
	movl	$pg1+3,4(%eax)
	movl	$pg2+3,8(%eax)
	movl	$pg3+3,12(%eax)
	# 倒序设置页表项
	movl	$16*1024*1024-4096+7,%eax				# 最后一页内存的地址。NOTE! 7: U/S - 1, R/W - 1, P - 1
	movl	$pg0+4096*4-4,%ebx						# 最后一个页表项的地址
	movl	$1024*4,%ecx
1:	movl	%eax,(%ebx)
	sub		$4096,%eax
	sub		$4,%ebx
	dec		%ecx
	cmpl	$0,%ecx
	jne		1b
	# 设置页表空间只读，方便调试
	movl	$5,%eax
	xorl	%ecx,%ecx
	movl	$0+5,%ebx								# 只读，U/S - 1, R/W - 0, P - 1
	movl	$pg0,%edx
1:	movl	%ebx,(%edx)
	addl	$4096,%ebx
	addl	$4,%edx
	inc		%ecx
	cmpw	%cx,%ax
	jne		1b
	# 开启分页模式。NOTE! CR3需要的是物理地址。
	xorl	%eax,%eax								# pg_dir is at 0x0000
	movl	%eax,%cr3								# 将CR3(3号32位控制寄存器,高20位存页目录基址)指向页目录表
	movl	%cr0,%eax
	orl		$0x80000000,%eax						# 设置启动使用分页处理（cr0 的PG 标志，位31）
	movl	%eax,%cr0								# 设置CR0中分页标志位PG为1
	ret												# NOTE! ret刷新指令缓冲流。

.align 4
setup_gdt:
	lgdt	gdt_new_48
	ret

.align 4
setup_idt:
	pushl	%edx
	pushl	%eax
	pushl	%ecx
	pushl	%edi
	lea		ignore_int,%edx
	movl	$0x00080000,%eax
	mov		%dx,%ax
	movw	$0x8e00,%dx			# 中断门类型，特权级0
	lea		idt,%edi
	movl	$256,%ecx
rp:	movl	%eax,(%edi)
	movl	%edx,4(%edi)
	addl	$8,%edi
	dec		%ecx
	cmpl	$0,%ecx
	jne		rp
	lidt	idt_new_48
	popl	%edi
	popl	%ecx
	popl	%eax
	popl	%edx
	ret

.align 4
write_char:
	pushl	%ebx
	pushl	%ecx
	pushl	%gs
	pushl	%ds
	movw	$SCRN_SEL,%bx
	mov		%bx,%gs
	movw	$0x10,%bx
	movw	%bx,%ds
	movl	$BOOTPOS,%ecx
	xor		%ebx,%ebx
	movw	%ds:(%ecx),%bx		# 取得光标位置，光标位置在boot中被保存
	shl		$1,%ebx
	movw	%ax,%gs:(%ebx)
	shr		$1,%ebx
	inc		%ebx
	cmpl	$2000,%ebx
	jne		1f
	movl	$0,%ebx
1:	movl	%ebx,%ds:(%ecx)		# 保存光标位置
	popl	%ds
	popl	%gs
	popl	%ecx
	popl	%ebx
	ret

.align 4
ignore_int:
	pushl	%ds
	pushl	%eax	
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	movl	$0x0449,%eax		# 字符"I"，属性：红色 黑底 不闪烁 不高亮
	movl	$0x10,%ebx
	mov		%bx,%ds
	call	write_char
	xorb	%al,%al
	inb		$0x64,%al			# 8042，测试64h端口位0，查看输出缓冲是否满，若满则读出一个字符
	andb	$0x01,%al
	cmpb	$0, %al
	je		1f
	inb		$0x60,%al			# 输出缓冲满，读出一个字符
	mov		$0x02,%ah
	movl	$0x10,%ebx
	mov		%bx,%ds
	call	write_char
# 以下注释的代码是屏蔽键盘输入，然后再允许，用于复位键盘输入。在8042中也可以不用
#	inb		$0x61,%al
#	orb		$0x80,%al
#	.word	0x00eb,0x00eb		# 此处是2条jmp $+2，$为当前指令地址，起延时作用，下同
#	outb	%al,$0x61
#	andb	$0x7f,%al
#	.word	0x00eb,0x00eb
#	outb	%al,$0x61
1:	movb	$0x20,%al			# 向8259A主芯片发送EOI命令,将其ISR中的相应位清零
	outb	%al,$0x20
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%eax
	popl	%ds
	iret

# ----------------------------------------------------

# GDT,IDT定义
.align 4
gdt_new_48:
	.word 256*8-1
	.long gdt

idt_new_48:
	.word 256*8-1
	.long idt

.align 8
#
# NOTE! 从0.3版开始，内核开启分页模式，内核虚拟地址空间在3GB-4GB，用户虚拟地址空间
# 为0GB-3GB，因此我们需要调整一下GDT :-)
#
gdt:
	.quad 0x0000000000000000
	.quad 0xc0c39a000000ffff	# 代码段，选择符0x08，基址0xC0000000，限长1GB，特权级0。
	.quad 0xc0c392000000ffff	# 数据段，选择符0x10，基址0xC0000000，限长1GB，特权级0。
	.quad 0xc0c0920b80000007	# 显示内存段，选择符0x18，基址0xC0000000+0xb8000，限长32KB，特权级0。
	.fill 252,8,0
# 0.3版以前的GDT
#gdt:
#	.quad 0x0000000000000000
#	.quad 0x00c09a0000000fff	# 代码段，选择符0x08，基址0，限长16MB，特权级0。
#	.quad 0x00c0920000000fff	# 数据段，选择符0x10，基址0，限长16MB，特权级0。
#	.quad 0x00c0920b80000007	# 显示内存段，选择符0x18，基址0xb8000，限长32KB，特权级0。
#	.fill 252,8,0

idt:
	.fill 256,8,0

# 内核初始化堆栈，也是后来任务0的用户栈
.align 4
	.fill 128,4,0
init_stack:
	.long init_stack
	.word 0x10
