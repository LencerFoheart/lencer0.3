#
# Small/kernel/system_call.s
# 
# (C) 2012-2013 Yafei Zheng
# V0.2 2013-01-31 23:18:14
#
# Email: e9999e@163.com, QQ: 1039332004
#

#
# 此文件包含部分系统调用以及中断处理程序。
#

SYS_CALL_COUNT		= 1			# 系统调用数

.globl interrupt_entry, ret_from_interrupt, system_call, keyboard_int, timer_int, hd_int
.globl system_call_22222, system_call_33333, system_call_44444


#
# 以下是中断入口函数和中断出口函数。每次中断或系统调用开始或结束时，都要调用。
# 进入中断时，我们需要保存eax的值，退出中断时，eax可能保存着返回值。故，我们将
# eax保存在栈顶，若eax保存有我们需要的返回值时，那么，我们只需要在调用中断退出
# 函数之前，改变栈顶的eax值即可，我们可以使用 movl %eax,%ss:(%esp) 实现。
#

# 中断入口函数。NOTE! 函数开始处将返回地址保存，在函数结束时重新压栈，以便返回。
.align 4
interrupt_entry:
	xchgl	%ebx,%ss:(%esp)		# ebx压栈，并换出返回地址
	pushl	%ecx
	pushl	%edx
	pushl	%ebp
	pushl	%esi
	pushl	%edi
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs
	pushl	%eax
	pushl	%ebx				# 返回地址压栈
	movw	$0x10,%bx			# 全局数据段选择符
	movw	%bx,%ds
	movw	%bx,%es
	movw	%bx,%fs
	movw	%bx,%gs
	movl	%ss:44(%esp),%ebx	# 恢复ebx
	ret

# 中断出口函数。这是一个正确的程序，下面注释掉的程序是一个经典的错误 :-)
.align 4
ret_from_interrupt:
	popl	%ebx				# 保存返回地址
	popl	%eax
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popl	%edi
	popl	%esi
	popl	%ebp
	popl	%edx
	popl	%ecx
	xchgl	%ebx,%ss:(%esp)		# 返回地址入栈，ebx弹出
	ret
#
# 中断出口函数。NOTE! 这是一个错误的程序!!! 该程序开始处跳过堆栈中的返回地址，
# 在函数结束时重新压栈，以便返回。貌似合情合理的逻辑，却隐含着这样一种错误：
# 程序一开始将返回地址跳过，试图在返回之前再得到返回地址将其压栈，但其他任何的
# 中断都可能在该程序正确的安置返回地址之前，将其返回地址覆盖掉!!!
#
#ret_from_interrupt:
#	addl	$4,%esp
#	popl	%eax
#	popl	%gs
#	popl	%fs
#	popl	%es
#	popl	%ds
#	popl	%edi
#	popl	%esi
#	popl	%ebp
#	popl	%edx
#	popl	%ecx
#	popl	%ebx
#	movl	%eax,%ss:-8(%esp)	# NOTE! 因为eax可能存储着返回值，故需将其保存
#	movl	%ss:-48(%esp),%eax	# NOTE! 48
#	pushl	%eax
#	movl	%ss:-4(%esp),%eax
#	ret

msg1:
	.ascii "system_call: nr of system call out of the limit!"
	.byte	0

.align 4
bad_system_call:
	lea		msg1,%eax
	pushl	%eax
	call	panic
	addl	$4,%esp				# NOTE! 实际上不可能执行到这里，but...
	iret

.align 4
system_call:
#	cmpl	$SYS_CALL_COUNT,%eax
#	jna		bad_system_call
	call	interrupt_entry
	call	sys_fork
	movl	%eax,%ss:(%esp)
	call	ret_from_interrupt
	iret

.align 4
system_call_22222:
	call	interrupt_entry
	pushl	%eax
	call	printf
	addl	$4,%esp
	call	ret_from_interrupt
	iret

.align 4
system_call_33333:
	call	interrupt_entry
	pushl	%eax
	pushl	$0
	call	bread
	addl	$8,%esp
	pushl	%eax
	call	brelse
	addl	$4,%esp
	call	ret_from_interrupt
	iret

.align 4
system_call_44444:
	call	interrupt_entry
	call	creat_write_read_close_file
	call	ret_from_interrupt
	iret

# 键盘中断处理程序
.align 4
keyboard_int:
	call	interrupt_entry
	xorb	%al,%al
	inb		$0x64,%al			# 8042，测试64h端口位0，查看输出缓冲是否满，若满则读出一个字符
	andb	$0x01,%al
	cmpb	$0, %al
	je		1f
	call	keyboard_hander		# 输出缓冲满，则调用键盘处理程序读出一个字符
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
	call	ret_from_interrupt
	iret

.align 4
timer_int:
	call	interrupt_entry
	movb	$0x20,%al			# 向8259A主芯片发送EOI命令,将其ISR中的相应位清零。NOTE! 须在do_timer前EOI
	outb	%al,$0x20
	movl	%ss:48(%esp),%eax	# 获取cs寄存器的值，以判断特权级
	pushl	%eax
	call	do_timer
	addl	$4,%esp
	call	ret_from_interrupt
	iret

#
# OK,we handle the slave '8259A' now. NOTE!!! The difference between the slave and master '8259A' is
# that the interrupt of slave '8259A' need EOI slave and master. But the interrupt of master is ONLY
# EOI master.
#
.align 4
hd_int:
	call	interrupt_entry
	movb	$0x20,%al			# 向8259A从芯片发送EOI命令,将其ISR中的相应位清零。
	outb	%al,$0xa0
	movb	$0x20,%al			# 向8259A主芯片发送EOI命令,将其ISR中的相应位清零。NOTE! 此处同时起到延时作用。
	outb	%al,$0x20
	movl	do_hd,%eax
	cmpl	$0,%eax
	jne		1f
	leal	hd_default_int,%eax
	movl	%eax,do_hd
1:	call	*do_hd				# NOTE!!! 此处call的是绝对地址，需加星号!!!
	call	ret_from_interrupt
	iret
