/*
 * Small/fs/buffer.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-02-22 08:44:01
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * 本文件包含缓冲区的相关处理程序。
 *
 * 缓冲区的起始地址是内核代码和数据的结束处。
 * NOTE! 缓冲区的起始地址和结束地址均在mm中设置。
 *
 * [??] 整个空闲缓冲区链表和哈希表需要加锁，目前还没有!!!
 */

//#ifndef __DEBUG__
//#define __DEBUG__
//#endif

#include "file_system.h"
#include "harddisk.h"
#include "kernel.h"
#include "stdio.h"
#include "string.h"
#include "sched.h"
#include "types.h"
#include "sys_set.h"

/*
 * 缓冲区开始和结束的物理地址。
 *
 * NOTE! buffer_mem_start, buffer_mem_end已在mm中初始化，故不能在本文件中再次初始化!!!
 */
unsigned long buffer_mem_start = 0, buffer_mem_end = 0;

static struct buffer_head *free_head = NULL;				// 缓冲区空闲表头部指针
static struct buffer_head *hash_table[NR_BUFF_HASH] = {0};	// 缓冲区哈希表指针数组

struct proc_struct * buff_wait = NULL;						// 指向等待缓冲区的进程


/*
 * 该函数为哈希表散列函数。根据设备号和块号，返回其在哈希表数组中的索引。
 * NOTE! 该函数并不知道该块是否存在于散列表。
 */
static int hash_fun(unsigned char dev, unsigned long nrblock)
{
	return (nrblock % NR_BUFF_HASH);
}

/*
 * 该函数检查哈希表，若指定块存在于哈希表，则返回指向该缓冲区的指针,否则，返回NULL.
 */
static struct buffer_head * scan_hash(unsigned char dev, unsigned long nrblock)
{
	int index = hash_fun(dev, nrblock);
	struct buffer_head *p = hash_table[index];

	while(p)
	{
		if(p->dev == dev && p->nrblock == nrblock)
		{
			return p;
		}
		p = p->p_hash_next;
		if(p == hash_table[index])
		{
			break;
		}
	}
	return (struct buffer_head *)NULL;
}

/*
 * 该函数从空闲表中摘取一个空闲缓冲区。输入若是NULL，则从头部摘取一个，否则根据输入的p值摘取。
 */
static struct buffer_head * get_from_free(struct buffer_head *p)
{
	if(p)						// 从表中摘下指定的空闲缓冲区
	{
		if(p->p_next_free == p)	// 表中只有一个缓冲区
		{
			free_head = NULL;
		}
		else
		{
			(p->p_pre_free)->p_next_free = p->p_next_free;
			(p->p_next_free)->p_pre_free = p->p_pre_free;
			if(p == free_head)		// p指向表头
			{
				free_head = free_head->p_next_free;
			}
		}
	}
	else
	{
		if(NULL == free_head)	// 表空
		{
			p = (struct buffer_head *)NULL;
		}
		else if(free_head->p_next_free == free_head)	// 表中只有一个缓冲区
		{
			p = free_head;
			free_head = NULL;
		}
		else					// 表中有多个缓冲区
		{
			p = free_head;
			(free_head->p_pre_free)->p_next_free = free_head->p_next_free;
			(free_head->p_next_free)->p_pre_free = free_head->p_pre_free;
			free_head = free_head->p_next_free;
		}
	}
	return p;
}

/*
 * 该函数将空闲缓冲区放入空闲表中。注意：正常情况是放入最后，当发生错误时，放入最前面。
 */
static void put_to_free(struct buffer_head *p, BOOL ishead)
{
	if(!p)
	{
		return;
	}

	if(NULL == free_head)	// 空闲表空
	{
		free_head = p;
		p->p_next_free = p;
		p->p_pre_free = p;
	}
	else
	{
		if(ishead)			// 放在头部
		{
			p->p_next_free = free_head;
			p->p_pre_free = free_head->p_pre_free;
			(free_head->p_pre_free)->p_next_free = p;
			free_head->p_pre_free = p;
			free_head = p;
		}
		else				// 放在末尾
		{
			p->p_pre_free = free_head->p_pre_free;
			(free_head->p_pre_free)->p_next_free = p;
			p->p_next_free = free_head;
			free_head->p_pre_free = p;
		}
	}
}

/* 从哈希表移除 */
static void remove_from_hash(struct buffer_head *p)
{
	if(!p || (p && !scan_hash(p->dev, p->nrblock)))
	{
		return;
	}

	int index = hash_fun(p->dev, p->nrblock);

	p->dev = p->nrblock = NULL;
	if(p->p_hash_next == p)	// 表中只有一个缓冲区
	{
		hash_table[index] = NULL;
	}
	else
	{
		(p->p_hash_pre)->p_hash_next = p->p_hash_next;
		(p->p_hash_next)->p_hash_pre = p->p_hash_pre;
		if(p == hash_table[index])		// p指向表头
		{
			hash_table[index] = hash_table[index]->p_hash_next;
		}
	}
}

/* 放入哈希表队列头部 */
static void put_to_hash(struct buffer_head *p, unsigned char newdev, unsigned long newnrblock)
{
	if(!p)
	{
		return;
	}

	int index = hash_fun(newdev, newnrblock);

	p->dev = newdev;
	p->nrblock = newnrblock;
	if(NULL == hash_table[index])
	{
		hash_table[index] = p;
		p->p_hash_next = p;
		p->p_hash_pre = p;
	}
	else
	{
		p->p_hash_next = hash_table[index];
		p->p_hash_pre = hash_table[index]->p_hash_pre;
		(hash_table[index]->p_hash_pre)->p_hash_next = p;
		hash_table[index]->p_hash_pre = p;
		hash_table[index] = p;
	}
}

struct buffer_head * getblk(unsigned char dev, unsigned long nrblock)
{
	struct buffer_head *p = NULL;

	while(1)
	{
		if(p = scan_hash(dev, nrblock))/* 在散列表 */
		{
			/* 在散列表，但上锁 */
			if(p->state & BUFF_LOCK)
			{
				sleep_on(&buff_wait, INTERRUPTIBLE);
				continue;		// 回到while循环
			}
			/* 在散列表，且空闲 */
			LOCK_BUFF(p);
			get_from_free(p);
			return p;
		}
		else/* 不在散列表 */
		{
			/* 不在散列表，且空闲表空 */
			if(NULL == free_head)
			{
				sleep_on(&buff_wait, INTERRUPTIBLE);
				continue;		// 回到while循环			
			}
			/* 不在散列表，在空闲表找到一个缓冲区，但标记了延迟写 */
			p = get_from_free((struct buffer_head *)NULL);
			if((p->state & BUFF_DELAY_W) && !(p->state & BUFF_LOCK))
			{
				/* 将缓冲区写入磁盘 */
				LOCK_BUFF(p);
				bwrite(p);
				continue;		// 回到while循环
			}
			/* 不在散列表，在空闲表找到一个缓冲区 */
			LOCK_BUFF(p);						// 上锁
			p->state &= (~BUFF_VAL) & 0xff;		// 使无效
			remove_from_hash(p);
			put_to_hash(p, dev, nrblock);
			return p;
		}
	}
}

void brelse(struct buffer_head *p)
{
	put_to_free(p, (p->state & BUFF_VAL) ? 0 : 1);
	(p->state & BUFF_VAL) ? 0 : remove_from_hash(p);
	UNLOCK_BUFF(p);
}

struct buffer_head * bread(unsigned char dev, unsigned long nrblock)
{
	struct buffer_head *p = getblk(dev, nrblock);
	if(!(p->state & BUFF_VAL))	// 无效，则启动磁盘读
	{
		/* 启动磁盘读 */
		insert_hd_request(p, WIN_READ, 1, nrblock);
		p->state |= BUFF_VAL;
	}

	/***** [DEBUG] ******/
//	for(int i=0; i<10; i++)
//	{
////		debug_delay();
//		d_printf("%X\t\t", *(unsigned char*)&p->p_data[i]);
//	}

	return p;
}

void bwrite(struct buffer_head *p)
{
	if(!(p->state & BUFF_LOCK))
	{
		LOCK_BUFF(p);
	}

	/* 启动磁盘写 */
	insert_hd_request(p, WIN_WRITE, 1, p->nrblock);
	if(p->state & BUFF_DELAY_W)
	{
		p->state &= (~BUFF_DELAY_W) & 0xff;
		UNLOCK_BUFF(p);
	}
	else
	{
		brelse(p);
	}
}

/*
 * NOTE! buff_init() 需在 mem_init() 之后被调用。
 */
void buff_init(void)
{
	int buffers = 0;						// 缓冲区数
	unsigned long tmp_next_free = 0;		// 指向下一个缓冲区，用于临时的保护性措施
	struct buffer_head *p = (struct buffer_head *)buffer_mem_start;	// 指向当前要处理缓冲区
	struct buffer_head *plast = NULL;		// 前一个缓冲区头部指针

	/* NOTE! 最后减1是为了避免，在640kb处无法存储完整的缓冲头部或缓冲数据区域 */
	buffers = (buffer_mem_end - buffer_mem_start - (1*1024*1024-640*1024)) / (BUFFER_SIZE + sizeof(struct buffer_head)) - 1;
	free_head = p;
	buff_wait = NULL;

//	d_printf("buff_head+BUFFER_SIZE = %d\n buffers = %d\n", sizeof(struct buffer_head)+BUFFER_SIZE, buffers);

	// 建立双向循环空闲缓冲区链表
	while(buffers--)
	{
		/*
		 * NOTE! 存在这样一种可能性错误：当 p 未达到640KB，而 p->p_next_free 已超过了，
		 * 所以此处不能再使用 p->p_next_free，而是采用临时变量去试探。
		 */
		tmp_next_free = (unsigned long)p + sizeof(struct buffer_head) + BUFFER_SIZE;
		// 当前缓冲区进入[640KB,1MB]区域
		if(tmp_next_free > 640*1024 && tmp_next_free < 1*1024*1024)
		{
			p = (struct buffer_head *)(1*1024*1024);
			/*
			 * Well! Here, we need to change the pointer of last-buffer.
			 * I debug it for a long time. :-)
			 */
			NULL==plast ? 0 : (plast->p_next_free = p);
			NULL==plast ? (free_head = p) : 0;
		}

		d_printf("[1] p = %d, p->p_next_free = %d\n", (unsigned long)p, (unsigned long)p->p_next_free);

		zeromem(p, sizeof(struct buffer_head));
		p->p_next_free = (struct buffer_head *)((unsigned long)p + sizeof(struct buffer_head) + BUFFER_SIZE);
		p->p_data = (unsigned char *)((unsigned long)p + sizeof(struct buffer_head));
		p->p_pre_free = plast;

		plast = p;
		p = p->p_next_free;

		d_printf("[2] p = %d, p->p_next_free = %d\n", (unsigned long)p, (unsigned long)p->p_next_free);

//		d_printf("next_free_buff_head_addr = %d\n", p->p_next_free);
//		debug_delay();
	}
	plast->p_next_free = free_head;
	free_head->p_pre_free = plast;

	// 初始化哈希链表
	for(int i=0; i<NR_BUFF_HASH; i++)
	{
		hash_table[i] = NULL;
	}
}