/*
 * Small/kernel/panic.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-02-20 14:34:01
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * ���ļ������ں˳�����������������ʹ�ں�ͣ�ڴ˴���
 * NOTE! ��ѭ��������������!!!
 */

#include "stdio.h"

void panic(const char * str)
{
	printf("Kernel panic: %s\n", str);

	for(;;)
		;
}