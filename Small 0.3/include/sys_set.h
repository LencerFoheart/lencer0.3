/*
 * Small/include/sys_set.h
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.2 2013-01-31 21:23:13
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * 此头文件包含系统常用的宏定义等。
 *
 * 主要包括：描述符表、端口操作、中断屏蔽、move_to_user_mode等的相关操作的宏定义。
 */

#ifndef _SYS_SET_H_
#define _SYS_SET_H_

//#include "sched.h"
/*
 * 进程ldt表项数。
 *
 * NOTE! 此宏已在"sched.h"中定义，为了避免头文件相互包含，采用此法解决。
 * 因为我还没有想到更好的解决头文件相互包含问题的办法 :-)
 */
#ifndef NR_PROC_LDT
#define NR_PROC_LDT		3
#endif

//**********************************************************************

#define move_to_user_mode() \
do \
{ \
	__asm__( \
	"pushfl\n\t" \
	"andl	$0xffffbfff,(%esp)\n\t" /* 复位标致寄存器的嵌套任务标志位NT */ \
	"popfl\n\t" \
	"movl	$0x17,%eax\n\t" /* 0x17：局部数据段选择符 */ \
	"mov	%ax,%ds\n\t" \
	"mov	%ax,%es\n\t" \		
	"mov	%ax,%fs\n\t" \
	"mov	%ax,%gs\n\t" \
	"mov	%esp,%eax\n\t" \
	"pushl	$0x17\n\t" \
	"pushl	%eax\n\t" \
	"pushfl\n\t" \
	"pushl	$0x0f\n\t" /* 0x0f：局部代码段选择符 */ \
	"pushl	$1f\n\t" \
	"iretl\n\t" \
	"1:\n\t"); \
}while(0)

#define cli()	__asm__("cli")
#define sti()	__asm__("sti")

// 从指定端口读入数据
#define in_b(src) ({ \
	unsigned char var; \
	__asm__( \
	"inb	%%dx,%%al\n\t" \
	"jmp	1f\n\t" \
	"1:		\n\t" \
	"jmp	1f\n\t" \
	"1:		\n\t" \
	: "=a"(var) : "d"(src) :);/*需放入 dx 中*/ \
var;})

// 向指定端口写数据
#define out_b(des, data) \
	__asm__( \
	"outb	%%al,%%dx\n\t" \
	"jmp	1f\n\t" \
	"1:		\n\t" \
	"jmp	1f\n\t" \
	"1:		\n\t" \
	: : "a"(data),"d"(des) : )/*需放入 dx 中*/

//**********************************************************************
// 以下用于设置描述符表

// 描述符结构体
typedef struct desc_struct {
	unsigned long a,b;
}desc_table[256];

extern desc_table idt,gdt;

/*
 * NOTE! 中断门和陷阱门的DPL用于访问保护。若检查DPL之后，具有访问权限，
 * 则CPU自动取门描述符中的段选择符载入CS。
 *
 * 因此，对于系统调用，内核将其特权级设置为RING3，使用户程序可以访问。
 * 而进入系统调用之后，CS的特权级为RING0，因为门描述符中的段选择符为0x8。
 */
#define INT_R0	0x8e00		// 中断门类型，特权级0
#define INT_R3	0xee00		// 中断门类型，特权级3
#define TRAP_R0	0x8f00		// 陷阱门类型，特权级0
#define TRAP_R3	0xef00		// 陷阱门类型，特权级3

// 设置中断门描述符
#define set_idt(FLAG,FUNC_ADDR,NR_INT) \
	__asm__( \
	"movl	%%eax,%%edx\n\t" \
	"movl	$0x00080000,%%eax\n\t"/* 注意：段选择符为 0x00080000 */ \
	"mov	%%dx,%%ax\n\t" \
	"movw	%%cx,%%dx\n\t" \
	"movl	%%eax,(%%edi)\n\t" \
	"movl	%%edx,4(%%edi)\n\t" \
	: : "c"(FLAG),"a"(FUNC_ADDR),"D"(&idt[NR_INT]) : ) // 注意: idt是中断描述符结构体数组，故不能再乘以8，另外注意是取地址。


// ldt和tss段选择符，特权级0
#define FIRST_LDT_ENTRY	4
#define FIRST_TSS_ENTRY	(FIRST_LDT_ENTRY+1)
#define _LDT(nr) ((unsigned long)(FIRST_LDT_ENTRY<<3) + (nr<<4))
#define _TSS(nr) ((unsigned long)(FIRST_TSS_ENTRY<<3) + (nr<<4))

/*
 * 设置GDT上的ldt和tss描述符。
 *
 * NOTE! ldt和tss段颗粒度为1byte。
 * ldt描述符类型为：0x82 (P - 1, DPL - 00, 0, TYPE - 0010)
 * tss描述符类型为：0x89 (P - 1, DPL - 00, 0, TYPE - 1001)
 * ldt段限长：NR_PROC_LDT * 8 - 1
 * tss段限长：sizeof(struct tss_struct) - 1
 *
 * NOTE! GCC编译选项： -fomit-frame-pointer --- 编译时的ERROR: can't find a register in class 'GENERAL_REGS' while reloading 'asm'.
 */
#define set_ldt_desc(nr,ldt_addr) \
do \
{ \
	unsigned long desc_addr = (unsigned long)&gdt + ((FIRST_LDT_ENTRY+(nr<<1)) << 3); /* gdt需取地址 */ \
	__asm__( \
	"movw	%%ax,%2\n\t" \
	"movl	%%eax,%4\n\t" \
	"movl	%%ebx,%3\n\t" \
	"movb	%5,%%al\n\t" \
	"movb	%%al,%6\n\t" \
	"movb	$0x82,%5\n\t" \
	::"a"(NR_PROC_LDT * 8 - 1), "b"(ldt_addr), "m"(*(char*)(desc_addr+0)), "m"(*(char*)(desc_addr+2)), \
	"m"(*(char*)(desc_addr+4)), "m"(*(char*)(desc_addr+5)), "m"(*(char*)(desc_addr+7)) :); \
}while(0)

#define set_tss_desc(nr,tss_addr) \
do \
{ \
	unsigned long desc_addr = (unsigned long)&gdt + ((FIRST_TSS_ENTRY+(nr<<1)) << 3); /* gdt需取地址 */ \
	__asm__( \
	"movw	%%ax,%2\n\t" \
	"movl	%%eax,%4\n\t" \
	"movl	%%ebx,%3\n\t" \
	"movb	%5,%%al\n\t" \
	"movb	%%al,%6\n\t" \
	"movb	$0x89,%5\n\t" \
	::"a"(sizeof(struct tss_struct) - 1), "b"(tss_addr), "m"(*(char*)(desc_addr+0)), "m"(*(char*)(desc_addr+2)), \
	"m"(*(char*)(desc_addr+4)), "m"(*(char*)(desc_addr+5)), "m"(*(char*)(desc_addr+7)) :); \
}while(0)

//**********************************************************************
#endif // _SYS_SET_H_