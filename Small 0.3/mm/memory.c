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

extern int _end;								// (&_end)Ϊ��������ݶν�����ַ��_end��������Ԥ���������

extern unsigned long buffer_mem_start;			// ��������ʼ��ַ
extern unsigned long buffer_mem_end;			// ������ĩ�˵�ַ


unsigned long high_mem = 0;						// �ڴ�߶˵�ַ
/*
 * �ڴ�ҳ��ʹ�������
 *
 * ӳ���ڴ淶Χ��0MB - 16MB����ֵ��ʾ��ҳ���ô�����0��ʾ��ҳ���С�
 */
unsigned char mem_map[MEM_MAP_SIZE] = {0};


/*
 * �ú�����������ڴ�ʹ�ñ�ȡ��һ���ڴ����ҳ�棬�����ڴ�ʹ�ñ���Ӧ���1��
 *
 * ���أ�ҳ����ʼ�����ַ����ΪNULL�����ʾ�޿���ҳ�档
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
 * �ú����ͷ�ָ��ҳ�棬�����ڴ�ʹ�ñ���Ӧ���1��
 *
 * ����Ϊ��ҳ�������ַ��
 */
void free_page(unsigned long page)
{
	if(page < buffer_mem_end || page > (high_mem - PAGE_SIZE))
	{
		panic("free_page: page address out of the limit.");
	}
	if(page & (PAGE_SIZE-1))	// �ڴ�ҳ����ʼ��ַ����PAGE_SIZE����
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
 * �ͷ�ҳ���ú����ͷ�ָ�����Ե�ַ�ռ���ڴ�ҳ�棬����ǰ����ҳĿ¼���ô�������1���򷵻ء�
 *
 * NOTE! ���ͷŶ���ҳ����ָ����ڴ�ҳ���Լ����ܵĶ���ҳ��ҳ�棬�����ͷ�ҳĿ¼������ҳ�档
 * �����ڸ������̹���һ���ں˵�ҳ���ʣ������ͷ��ں˿ռ䡣
 *
 * ����Ϊ����ʼ���Ե�ַ���ڴ�ռ��С�����أ���δ�����ͷţ��򷵻�FALSE��
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

	if(mem_map[current->tss.cr3 / PAGE_SIZE] > 1)	// ����ǰ����ҳĿ¼���ô�������1���򷵻�FALSE
	{
		return FALSE;
	}

	unsigned long to = from + size;
	unsigned long addr = 0;
	// �ͷŶ���ҳ����ָ���ҳ��
	for(; from < to; from += PAGE_SIZE)
	{
		addr = current->tss.cr3 + from / (PAGE_SIZE/4*PAGE_SIZE) * 4;	// �õ�ҳĿ¼����ĵ�ַ
		addr &= 0xfffff000;
		if(*(unsigned long *)addr & 1)									// ҳĿ¼������Pλ��λ
		{
			addr = (*(unsigned long *)addr >> 12) & 0xffffffff;			// ʹ��ָ�����ҳ������ҳ����׵�ַ
			addr = addr + (from % (PAGE_SIZE/4*PAGE_SIZE)) / PAGE_SIZE * 4;
			if(*(unsigned long *)addr & 1)								// ����ҳ������Pλ��λ
			{
				free_page((*(unsigned long *)addr >> 12) & 0xffffffff);
				*(unsigned long *)addr = 0;
			}
		}
	}
	// �ͷſ��ܵĶ���ҳ��ҳ��
	from = to - size;
	unsigned long begin = 0, end = 0;
	unsigned char isfree = 0;					// ָʾ�ö���ҳ��ҳ���Ƿ�����ͷţ����ö���ҳ��ҳ��ȫ������
	for(; from < to; from += PAGE_SIZE/4*PAGE_SIZE, from &= 0xff400000)
	{
		isfree = 0;
		if(!(from & (PAGE_SIZE/4*PAGE_SIZE-1)) && from <= to-PAGE_SIZE)	// ����4MB����,�Ҳ�������Χ
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
			for(; begin < end; begin += PAGE_SIZE)	// ��������Ķ���ҳ�����ȫ��Ϊ�գ����ͷŸö���ҳ��ҳ��
			{
				addr = current->tss.cr3 + begin / (PAGE_SIZE/4*PAGE_SIZE) * 4;	// �õ�ҳĿ¼����ĵ�ַ
				addr &= 0xfffff000;
				if(*(unsigned long *)addr & 1)									// ҳĿ¼������Pλ��λ
				{
					addr = (*(unsigned long *)addr >> 12) & 0xffffffff;			// ʹ��ָ�����ҳ������ҳ����׵�ַ
					addr = addr + (begin % (PAGE_SIZE/4*PAGE_SIZE)) / PAGE_SIZE * 4;
					if(*(unsigned long *)addr & 1)								// ����ҳ������Pλ��λ
					{
						break;
					}
				}
			}
			(begin < end) ? 0 : (isfree=1, begin=from&0xff400000);
		}
		if(isfree)	// �ͷŸö���ҳ��ҳ��
		{
			addr = current->tss.cr3 + begin / (PAGE_SIZE/4*PAGE_SIZE) * 4;	// �õ�ҳĿ¼����ĵ�ַ
			addr &= 0xfffff000;
			if(*(unsigned long *)addr & 1)									// ҳĿ¼������Pλ��λ
			{
				free_page((*(unsigned long *)addr >> 12) & 0xffffffff);
				*(unsigned long *)addr = 0;
			}
		}
	}

	return TRUE;
}

/*
 * ��¡ҳ���ú�����¡����ҳ��ʹ��ҳĿ¼ҳ�����ô�����1.
 */
void clone_page_tables(long * cr3)
{
	*cr3 = current->tss.cr3;
	mem_map[(*cr3) / PAGE_SIZE]++;
}

/*
 * �ú�����ҳ�����ڴ�ҳ��ҽӡ�
 *
 * page: ҳ����ʼ�����ַ; v_addr: ҳ��ӳ����������Ե�ַ. �����󣬷���FALSE.
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
	if(page & (PAGE_SIZE-1))	// �ڴ�ҳ����ʼ��ַ����PAGE_SIZE����
	{
		panic("put_page: page address with wrong alignment.");
	}
	if(v_addr & (PAGE_SIZE-1))	// �������Ե�ַ����PAGE_SIZE����
	{
		panic("put_page: v_addr with wrong alignment.");
	}

	unsigned long put_addr = current->tss.cr3 + v_addr / (PAGE_SIZE/4*PAGE_SIZE) * 4;	// ��Ҫ���õ�ҳĿ¼����ĵ�ַ
	put_addr &= 0xfffff000;
	if(!(*(unsigned long *)put_addr & 1))		// ҳĿ¼������Pλ�Ƿ���λ
	{
		unsigned long tmp = 0;
		if(NULL == (tmp = get_free_page()))
		{
			return FALSE;
		}
		*(unsigned long *)put_addr = tmp + 7;	// 7: U/S - 1, R/W - 1, P - 1
	}
	put_addr = (*(unsigned long *)put_addr >> 12) & 0xffffffff;		// ʹ��ָ�����ҳ������ҳ����׵�ַ
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
	// �ڴ沼��
	high_mem = (*(unsigned short *)0x90002 + 1024) * 1024;		// 0x90002��ַ����������չ�ڴ��С
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
	
	// ��ʼ��mem_map
	memset(mem_map, (char)USED, MEM_MAP_SIZE);
	/*
	 * [??] NOTE! &�����ȼ�С��+���� (&mem_map) + buffer_mem_end/PAGE_SIZE*sizeof(unsigned char) ���ǻ����!!!
	 * I don't know why at present :-(
	 */
	zeromem(&mem_map[buffer_mem_end/PAGE_SIZE*sizeof(unsigned char)], (high_mem - buffer_mem_end) / PAGE_SIZE * sizeof(unsigned char));
}