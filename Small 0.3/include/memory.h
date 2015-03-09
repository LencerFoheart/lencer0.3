/*
 * Small/include/memory.h
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-02-19 17:35:03
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "types.h"

//**********************************************************************

#define PAGE_SIZE		4096				// 4kb
#define MEM_MAP_SIZE	(16*1024*1024/PAGE_SIZE)			// 0MB - 16MB
#define USED			100					// 页面已使用

/*
 * 以下是用户虚拟地址空间范围(0GB - 3GB)和内核虚拟地址空间范围(3GB - 4GB)。
 *
 * NOTE! 以下声明用于内存管理，需与GDT,LDT等其他地方保持一致。
 */
#define V_USER_ZONE_START		0x0
#define V_USER_ZONE_END			0xC0000000
#define V_KERNEL_ZONE_START		0xC0000000
#define V_KERNEL_ZONE_END		0x100000000

//**********************************************************************
extern unsigned long pg_dir[PAGE_SIZE/4];

extern unsigned long get_free_page(void);
extern void free_page(unsigned long page);
extern BOOL free_page_tables(unsigned long from, unsigned int size);
extern void clone_page_tables(long * cr3);
extern BOOL put_page(unsigned long page, unsigned long v_addr);
extern void mem_init(void);

//**********************************************************************

#endif // _MEMORY_H_