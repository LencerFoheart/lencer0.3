/*
 * Small/kernel/panic.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-02-20 14:34:01
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * 此文件用于内核出现致命错误，其结果是使内核停在此处。
 * NOTE! 死循环，但不是死机!!!
 */

#include "stdio.h"

void panic(const char * str)
{
	printf("Kernel panic: %s\n", str);

	for(;;)
		;
}