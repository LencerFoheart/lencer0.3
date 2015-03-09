/*
 * Small/mm/memory.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-02-22 00:55:26
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

#include "memory.h"
#include "string.h"
#include "stdio.h"
#include "kernel.h"
#include "sched.h"
#include "sys_nr.h"
#include "types.h"

extern int _end;								// (&_end)为代码和数据段结束地址。_end链接器的预定义变量。

extern unsigned long buffer_mem_start;			// 缓冲区起始地址
extern unsigned long buffer_mem_end;			// 缓冲区末端地址


unsigned long high_mem = 0;						// 内存高端地址
/*
 * 内存页面使用情况表。
 *
 * 映射内存范围：0MB - 16MB。其值表示该页引用次数，0表示该页空闲。
 */
unsigned char mem_map[MEM_MAP_SIZE] = {0};


/*
 * 该函数倒序查找内存使用表，取得一个内存空闲页面，并将内存使用表相应项加1。
 *
 * 返回：页面起始物理地址，若为NULL，则表示无空闲页面。
 */
unsigned long get_free_page(void)
{
	int i = 0;
	for(i=high_mem/PAGE_SIZE-1; i>=buffer_mem_end/PAGE_SIZE; i--)
	{
		if(0 == mem_map[i])
		{
			++mem_map[i];
			break;
		}
	}
	return (i>=buffer_mem_end/PAGE_SIZE) ? (i*PAGE_SIZE) : NULL;
}

/*
 * 该函数释放指定页面，并将内存使用表相应项减1。
 *
 * 输入为：页面物理地址。
 */
void free_page(unsigned long page)
{
	if(page < buffer_mem_end || page > (high_mem - PAGE_SIZE))
	{
		panic("free_page: page address out of the limit.");
	}
	if(page & (PAGE_SIZE-1))	// 内存页面起始地址不是PAGE_SIZE对齐
	{
		panic("free_page: page address with wrong alignment.");
	}

	if(0 == mem_map[page / PAGE_SIZE])
	{
		panic("free_page: trying to free the free-page.");
	}
	--mem_map[page / PAGE_SIZE];
}

/*
 * 释放页表。该函数释放指定线性地址空间的内存页面，若当前进程页目录引用次数大于1，则返回。
 *
 * NOTE! 仅释放二级页表项指向的内存页面以及可能的二级页表页面，而不释放页目录表所在页面。
 * 另，由于各个进程共用一套内核的页表，故，不能释放内核空间。
 *
 * 输入为：起始线性地址、内存空间大小。返回：若未进行释放，则返回FALSE。
 */
BOOL free_page_tables(unsigned long from, unsigned int size)
{
	if(from & (PAGE_SIZE-1))
	{
		panic("free_page_tables: the start address with wrong alignment.");
	}
	size &= 0xfffff000;
	if(from + size > V_USER_ZONE_END)
	{
		panic("free_page_tables: trying to free kernel address space.");
	}

	if(mem_map[current->tss.cr3 / PAGE_SIZE] > 1)	// 若当前进程页目录引用次数大于1，则返回FALSE
	{
		return FALSE;
	}

	unsigned long to = from + size;
	unsigned long addr = 0;
	// 释放二级页表项指向的页面
	for(; from < to; from += PAGE_SIZE)
	{
		addr = current->tss.cr3 + from / (PAGE_SIZE/4*PAGE_SIZE) * 4;	// 得到页目录表项的地址
		addr &= 0xfffff000;
		if(*(unsigned long *)addr & 1)									// 页目录表项中P位置位
		{
			addr = (*(unsigned long *)addr >> 12) & 0xffffffff;			// 使其指向二级页表所在页面的首地址
			addr = addr + (from % (PAGE_SIZE/4*PAGE_SIZE)) / PAGE_SIZE * 4;
			if(*(unsigned long *)addr & 1)								// 二级页表项中P位置位
			{
				free_page((*(unsigned long *)addr >> 12) & 0xffffffff);
				*(unsigned long *)addr = 0;
			}
		}
	}
	// 释放可能的二级页表页面
	from = to - size;
	unsigned long begin = 0, end = 0;
	unsigned char isfree = 0;					// 指示该二级页表页面是否可以释放，即该二级页表页面全部空闲
	for(; from < to; from += PAGE_SIZE/4*PAGE_SIZE, from &= 0xff400000)
	{
		isfree = 0;
		if(!(from & (PAGE_SIZE/4*PAGE_SIZE-1)) && from <= to-PAGE_SIZE)	// 若是4MB对齐,且不超出范围
		{
			isfree = 1;
			begin = from;
		}
		else
		{
			if(!(from & (PAGE_SIZE/4*PAGE_SIZE-1)))
			{
				begin = from & 0xff400000;
				end = from;
			}
			else	// from > to-PAGE_SIZE
			{
				begin = to;
				end = (to + PAGE_SIZE/4*PAGE_SIZE) & 0xff400000;
			}
			for(; begin < end; begin += PAGE_SIZE)	// 遍历其余的二级页表项，若全部为空，则释放该二级页表页面
			{
				addr = current->tss.cr3 + begin / (PAGE_SIZE/4*PAGE_SIZE) * 4;	// 得到页目录表项的地址
				addr &= 0xfffff000;
				if(*(unsigned long *)addr & 1)									// 页目录表项中P位置位
				{
					addr = (*(unsigned long *)addr >> 12) & 0xffffffff;			// 使其指向二级页表所在页面的首地址
					addr = addr + (begin % (PAGE_SIZE/4*PAGE_SIZE)) / PAGE_SIZE * 4;
					if(*(unsigned long *)addr & 1)								// 二级页表项中P位置位
					{
						break;
					}
				}
			}
			(begin < end) ? 0 : (isfree=1, begin=from&0xff400000);
		}
		if(isfree)	// 释放该二级页表页面
		{
			addr = current->tss.cr3 + begin / (PAGE_SIZE/4*PAGE_SIZE) * 4;	// 得到页目录表项的地址
			addr &= 0xfffff000;
			if(*(unsigned long *)addr & 1)									// 页目录表项中P位置位
			{
				free_page((*(unsigned long *)addr >> 12) & 0xffffffff);
				*(unsigned long *)addr = 0;
			}
		}
	}

	return TRUE;
}

/*
 * 克隆页表。该函数克隆进程页表，使其页目录页面引用次数加1.
 */
void clone_page_tables(long * cr3)
{
	*cr3 = current->tss.cr3;
	mem_map[(*cr3) / PAGE_SIZE]++;
}

/*
 * 该函数将页表与内存页面挂接。
 *
 * page: 页面起始物理地址; v_addr: 页面映射的虚拟线性地址. 若错误，返回FALSE.
 */
BOOL put_page(unsigned long page, unsigned long v_addr)
{
	if(page < buffer_mem_end || page > (high_mem - PAGE_SIZE))
	{
		panic("put_page: page address out of the limit.");
	}
	if(v_addr > (V_USER_ZONE_END - PAGE_SIZE))
	{
		panic("put_page: v_addr out of the limit.");
	}
	if(page & (PAGE_SIZE-1))	// 内存页面起始地址不是PAGE_SIZE对齐
	{
		panic("put_page: page address with wrong alignment.");
	}
	if(v_addr & (PAGE_SIZE-1))	// 虚拟线性地址不是PAGE_SIZE对齐
	{
		panic("put_page: v_addr with wrong alignment.");
	}

	unsigned long put_addr = current->tss.cr3 + v_addr / (PAGE_SIZE/4*PAGE_SIZE) * 4;	// 将要放置的页目录表项的地址
	put_addr &= 0xfffff000;
	if(!(*(unsigned long *)put_addr & 1))		// 页目录表项中P位是否置位
	{
		unsigned long tmp = 0;
		if(NULL == (tmp = get_free_page()))
		{
			return FALSE;
		}
		*(unsigned long *)put_addr = tmp + 7;	// 7: U/S - 1, R/W - 1, P - 1
	}
	put_addr = (*(unsigned long *)put_addr >> 12) & 0xffffffff;		// 使其指向二级页表所在页面的首地址
	put_addr = put_addr + (v_addr % (PAGE_SIZE/4*PAGE_SIZE)) / PAGE_SIZE * 4;
	*(unsigned long *)put_addr = page + 7;

	return TRUE;
}

void do_no_page(void)
{
	panic("do_no_page: page fault.");
}

void mem_init(void)
{
	// 内存布局
	high_mem = (*(unsigned short *)0x90002 + 1024) * 1024;		// 0x90002地址处保存着扩展内存大小
	high_mem &= 0xfffff000;
	if(high_mem >= 16*1024*1024)
	{
		high_mem = 16*1024*1024;
	}
	if(high_mem < 10*1024*1024)
	{
		panic("the size of memory can't below 10MB.");
	}
	buffer_mem_end = 3*1024*1024;
	buffer_mem_start = (unsigned long)&_end;
	
	// 初始化mem_map
	memset(mem_map, (char)USED, MEM_MAP_SIZE);
	/*
	 * [??] NOTE! &的优先级小于+，但 (&mem_map) + buffer_mem_end/PAGE_SIZE*sizeof(unsigned char) 还是会出错!!!
	 * I don't know why at present :-(
	 */
	zeromem(&mem_map[buffer_mem_end/PAGE_SIZE*sizeof(unsigned char)], (high_mem - buffer_mem_end) / PAGE_SIZE * sizeof(unsigned char));
}