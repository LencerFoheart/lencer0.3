/*
 * Small/include/kernel.h
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-02-20 14:48:09
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * 此文件包含内核的一些普通函数。
 *
 * 主要包括：panic, debug, fork ...
 */

#ifndef _KERNEL_H_
#define _KERNEL_H_

//**********************************************************************

/* panic */
extern void panic(const char * str);


/* debug */

//#ifndef __DEBUG__
//#define __DEBUG__
//#endif

#ifdef __DEBUG__
#define d_printf printf
#else
#define d_printf no_printf
#endif

#define DELAY_COUNT	100000		// 空循环 DELAY_COUNT*10000 次

extern void debug_delay(void);
extern void no_printf(char * fmt, ...);

#define DEBUG_DELAY() \
do \
{ \
	for(int i=0; i<DELAY_COUNT; i++) \
		for(int j=0; j<10000; j++) \
			; \
}while(0)


/* fork */
extern int sys_fork(long none, long gs, long fs, long es, long ds, 
			 long edi, long esi, long ebp, long edx, long ecx, long ebx,
			 long eip, long cs, long eflags, long esp, long ss);
extern void copy_process(int nr, long gs, long fs, long es, long ds, 
				long edi, long esi, long ebp, long edx, long ecx, long ebx,
				long eip, long cs, long eflags, long esp, long ss);
extern int find_empty_process(void);

/* trap */
extern void trap_init(void);

//**********************************************************************

#endif // _KERNEL_H_