#
# Small/kernel/system_call.s
# 
# (C) 2012-2013 Yafei Zheng
# V0.2 2013-01-31 23:18:14
#
# Email: e9999e@163.com, QQ: 1039332004
#

#
# ���ļ���������ϵͳ�����Լ��жϴ������
#

SYS_CALL_COUNT		= 1			# ϵͳ������

.globl interrupt_entry, ret_from_interrupt, system_call, keyboard_int, timer_int, hd_int
.globl system_call_22222, system_call_33333, system_call_44444


#
# �������ж���ں������жϳ��ں�����ÿ���жϻ�ϵͳ���ÿ�ʼ�����ʱ����Ҫ���á�
# �����ж�ʱ��������Ҫ����eax��ֵ���˳��ж�ʱ��eax���ܱ����ŷ���ֵ���ʣ����ǽ�
# eax������ջ������eax������������Ҫ�ķ���ֵʱ����ô������ֻ��Ҫ�ڵ����ж��˳�
# ����֮ǰ���ı�ջ����eaxֵ���ɣ����ǿ���ʹ�� movl %eax,%ss:(%esp) ʵ�֡�
#

# �ж���ں�����NOTE! ������ʼ�������ص�ַ���棬�ں�������ʱ����ѹջ���Ա㷵�ء�
.align 4
interrupt_entry:
	xchgl	%ebx,%ss:(%esp)		# ebxѹջ�����������ص�ַ
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
	pushl	%ebx				# ���ص�ַѹջ
	movw	$0x10,%bx			# ȫ�����ݶ�ѡ���
	movw	%bx,%ds
	movw	%bx,%es
	movw	%bx,%fs
	movw	%bx,%gs
	movl	%ss:44(%esp),%ebx	# �ָ�ebx
	ret

# �жϳ��ں���������һ����ȷ�ĳ�������ע�͵��ĳ�����һ������Ĵ��� :-)
.align 4
ret_from_interrupt:
	popl	%ebx				# ���淵�ص�ַ
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
	xchgl	%ebx,%ss:(%esp)		# ���ص�ַ��ջ��ebx����
	ret
#
# �жϳ��ں�����NOTE! ����һ������ĳ���!!! �ó���ʼ��������ջ�еķ��ص�ַ��
# �ں�������ʱ����ѹջ���Ա㷵�ء�ò�ƺ��������߼���ȴ����������һ�ִ���
# ����һ��ʼ�����ص�ַ��������ͼ�ڷ���֮ǰ�ٵõ����ص�ַ����ѹջ���������κε�
# �ж϶������ڸó�����ȷ�İ��÷��ص�ַ֮ǰ�����䷵�ص�ַ���ǵ�!!!
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
#	movl	%eax,%ss:-8(%esp)	# NOTE! ��Ϊeax���ܴ洢�ŷ���ֵ�����轫�䱣��
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
	addl	$4,%esp				# NOTE! ʵ���ϲ�����ִ�е����but...
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

# �����жϴ������
.align 4
keyboard_int:
	call	interrupt_entry
	xorb	%al,%al
	inb		$0x64,%al			# 8042������64h�˿�λ0���鿴��������Ƿ��������������һ���ַ�
	andb	$0x01,%al
	cmpb	$0, %al
	je		1f
	call	keyboard_hander		# ���������������ü��̴���������һ���ַ�
# ����ע�͵Ĵ��������μ������룬Ȼ�����������ڸ�λ�������롣��8042��Ҳ���Բ���
#	inb		$0x61,%al
#	orb		$0x80,%al
#	.word	0x00eb,0x00eb		# �˴���2��jmp $+2��$Ϊ��ǰָ���ַ������ʱ���ã���ͬ
#	outb	%al,$0x61
#	andb	$0x7f,%al
#	.word	0x00eb,0x00eb
#	outb	%al,$0x61
1:	movb	$0x20,%al			# ��8259A��оƬ����EOI����,����ISR�е���Ӧλ����
	outb	%al,$0x20
	call	ret_from_interrupt
	iret

.align 4
timer_int:
	call	interrupt_entry
	movb	$0x20,%al			# ��8259A��оƬ����EOI����,����ISR�е���Ӧλ���㡣NOTE! ����do_timerǰEOI
	outb	%al,$0x20
	movl	%ss:48(%esp),%eax	# ��ȡcs�Ĵ�����ֵ�����ж���Ȩ��
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
	movb	$0x20,%al			# ��8259A��оƬ����EOI����,����ISR�е���Ӧλ���㡣
	outb	%al,$0xa0
	movb	$0x20,%al			# ��8259A��оƬ����EOI����,����ISR�е���Ӧλ���㡣NOTE! �˴�ͬʱ����ʱ���á�
	outb	%al,$0x20
	movl	do_hd,%eax
	cmpl	$0,%eax
	jne		1f
	leal	hd_default_int,%eax
	movl	%eax,do_hd
1:	call	*do_hd				# NOTE!!! �˴�call���Ǿ��Ե�ַ������Ǻ�!!!
	call	ret_from_interrupt
	iret
