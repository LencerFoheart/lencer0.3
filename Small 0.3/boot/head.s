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
# ��������GNU as 2.21.1
#
# head.s����Ҫ�������£�
#	1.��������GDT��IDT���ں˶�ջ������0�û�̬��ջ��
#	2.����Ĭ���ж�
#	3.����ҳ��������ҳģʽ
# **************************************************************************************************

BOOTPOS		= 0x90000			# �����������ƶ�����λ�á��� INITSEG * 16
SCRN_SEL	= 0x18				# ��Ļ��ʾ�ڴ��ѡ���
KERNEL_PG_DIR_START	= 768		# �ں˵�ҳĿ¼�Ŀ�ʼ�NOTE! �ں�ռ�õ������ַ�ռ�Ϊ��3GB - 4GB.

.text

.globl startup_32
.globl pg_dir, idt, gdt, init_stack

pg_dir:							# ����0��ҳĿ¼��
startup_32:
	movl	$0x10,%eax
	mov		%ax,%ds
	mov		%ax,%es
	mov		%ax,%fs
	mov		%ax,%gs
	lss		init_stack,%esp
	jmp		after_page

# ----------------------------------------------------

# �����ǽ���0��ҳ��Ŀǰֻ֧��16MB�ڴ棬��ҳ��ֻ��4ҳ��
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
# ���ý���0��ҳĿ¼��ҳ����������ҳģʽ��
# NOTE! �򿪷�ҳģʽ֮�������ں�����ռ�Ϊ3GB-4GB����Ŀǰ�Ķλ�ַ����0x00��
# �ʶ�GDT�����������Ҫ���Ͻ���! ��Ϊ�˴���ʹ���ŷǹ���ҳĿ¼����0�ʼ��ҳĿ¼����
	call	setup_page
# ���µ�λ����������GDT,IDT
	call	setup_gdt
	call	setup_idt
	movl	$0x10,%eax
	mov		%ax,%ds
	mov		%ax,%es
	mov		%ax,%fs
	mov		%ax,%gs
	lss		init_stack,%esp

# ��8259A�ر�̣��������жϺ�Ϊint 0x20-0x2F���˴������linux-0.11
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
	# ���������ע�ͣ��˴��������ж�
#	mov		$0xFF,%al		# mask off %all interrupts for now
#	out		%al,$0x21
#	.word	0x00eb,0x00eb
#	out		%al,$0xA1

# OK! We begin to run the MAIN function now.
	pushl	$main
	ret						# ����main����ִ��

# ----------------------------------------------------

#
# ���ý���0��ҳĿ¼���ҳ�����������ҳģʽ��NOTE! �ں�Ĭ�ϵ��ڴ��СΪ16MB��
#
# NOTE! ����0ҳĿ¼��������û�̬ҳĿ¼����ں�̬ҳĿ¼���
#
.align 4
setup_page:
	# ѭ������
	movl	$1024*5,%ecx							# 5 pages - pg_dir+4 page tables
	xorl	%eax,%eax
	xorl	%edi,%edi								# pg_dir is at 0x000
	cld; rep; stosl
	# �����û�̬ҳĿ¼���U/S - 1��NOTE! ����������GDT֮ǰ����Ҫ��ЩҳĿ¼��
	movl	$0*4+pg_dir,%eax						# �û�̬��ҳĿ¼���ַ
	movl	$pg0+7,0(%eax)							# U/S - 1, R/W - 1, P - 1
	movl	$pg1+7,4(%eax)
	movl	$pg2+7,8(%eax)
	movl	$pg3+7,12(%eax)
	# �����ں�̬ҳĿ¼���U/S - 0���ں�����ռ���3GB-4GB.
	movl	$KERNEL_PG_DIR_START*4+pg_dir,%eax		# �ں�̬��ҳĿ¼���ַ
	movl	$pg0+3,0(%eax)							# U/S - 0, R/W - 1, P - 1
	movl	$pg1+3,4(%eax)
	movl	$pg2+3,8(%eax)
	movl	$pg3+3,12(%eax)
	# ��������ҳ����
	movl	$16*1024*1024-4096+7,%eax				# ���һҳ�ڴ�ĵ�ַ��NOTE! 7: U/S - 1, R/W - 1, P - 1
	movl	$pg0+4096*4-4,%ebx						# ���һ��ҳ����ĵ�ַ
	movl	$1024*4,%ecx
1:	movl	%eax,(%ebx)
	sub		$4096,%eax
	sub		$4,%ebx
	dec		%ecx
	cmpl	$0,%ecx
	jne		1b
	# ����ҳ��ռ�ֻ�����������
	movl	$5,%eax
	xorl	%ecx,%ecx
	movl	$0+5,%ebx								# ֻ����U/S - 1, R/W - 0, P - 1
	movl	$pg0,%edx
1:	movl	%ebx,(%edx)
	addl	$4096,%ebx
	addl	$4,%edx
	inc		%ecx
	cmpw	%cx,%ax
	jne		1b
	# ������ҳģʽ��NOTE! CR3��Ҫ���������ַ��
	xorl	%eax,%eax								# pg_dir is at 0x0000
	movl	%eax,%cr3								# ��CR3(3��32λ���ƼĴ���,��20λ��ҳĿ¼��ַ)ָ��ҳĿ¼��
	movl	%cr0,%eax
	orl		$0x80000000,%eax						# ��������ʹ�÷�ҳ����cr0 ��PG ��־��λ31��
	movl	%eax,%cr0								# ����CR0�з�ҳ��־λPGΪ1
	ret												# NOTE! retˢ��ָ�������

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
	movw	$0x8e00,%dx			# �ж������ͣ���Ȩ��0
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
	movw	%ds:(%ecx),%bx		# ȡ�ù��λ�ã����λ����boot�б�����
	shl		$1,%ebx
	movw	%ax,%gs:(%ebx)
	shr		$1,%ebx
	inc		%ebx
	cmpl	$2000,%ebx
	jne		1f
	movl	$0,%ebx
1:	movl	%ebx,%ds:(%ecx)		# ������λ��
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
	movl	$0x0449,%eax		# �ַ�"I"�����ԣ���ɫ �ڵ� ����˸ ������
	movl	$0x10,%ebx
	mov		%bx,%ds
	call	write_char
	xorb	%al,%al
	inb		$0x64,%al			# 8042������64h�˿�λ0���鿴��������Ƿ��������������һ���ַ�
	andb	$0x01,%al
	cmpb	$0, %al
	je		1f
	inb		$0x60,%al			# ���������������һ���ַ�
	mov		$0x02,%ah
	movl	$0x10,%ebx
	mov		%bx,%ds
	call	write_char
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
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%eax
	popl	%ds
	iret

# ----------------------------------------------------

# GDT,IDT����
.align 4
gdt_new_48:
	.word 256*8-1
	.long gdt

idt_new_48:
	.word 256*8-1
	.long idt

.align 8
#
# NOTE! ��0.3�濪ʼ���ں˿�����ҳģʽ���ں������ַ�ռ���3GB-4GB���û������ַ�ռ�
# Ϊ0GB-3GB�����������Ҫ����һ��GDT :-)
#
gdt:
	.quad 0x0000000000000000
	.quad 0xc0c39a000000ffff	# ����Σ�ѡ���0x08����ַ0xC0000000���޳�1GB����Ȩ��0��
	.quad 0xc0c392000000ffff	# ���ݶΣ�ѡ���0x10����ַ0xC0000000���޳�1GB����Ȩ��0��
	.quad 0xc0c0920b80000007	# ��ʾ�ڴ�Σ�ѡ���0x18����ַ0xC0000000+0xb8000���޳�32KB����Ȩ��0��
	.fill 252,8,0
# 0.3����ǰ��GDT
#gdt:
#	.quad 0x0000000000000000
#	.quad 0x00c09a0000000fff	# ����Σ�ѡ���0x08����ַ0���޳�16MB����Ȩ��0��
#	.quad 0x00c0920000000fff	# ���ݶΣ�ѡ���0x10����ַ0���޳�16MB����Ȩ��0��
#	.quad 0x00c0920b80000007	# ��ʾ�ڴ�Σ�ѡ���0x18����ַ0xb8000���޳�32KB����Ȩ��0��
#	.fill 252,8,0

idt:
	.fill 256,8,0

# �ں˳�ʼ����ջ��Ҳ�Ǻ�������0���û�ջ
.align 4
	.fill 128,4,0
init_stack:
	.long init_stack
	.word 0x10
