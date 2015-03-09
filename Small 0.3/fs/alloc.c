/*
 * Small/fs/alloc.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-3-2 18:54:22
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * 此文件包含磁盘索引节点和磁盘块的分配以及释放。
 *
 * [??] 超级块应定期写入磁盘，目前还没实现该功能。
 * [??] 超级块是否可延迟写，尚需思考!!!
 */

//#ifndef __DEBUG__
//#define __DEBUG__
//#endif

#include "file_system.h"
#include "kernel.h"
#include "stdio.h"
#include "string.h"
#include "types.h"
#include "sched.h"

struct m_super_block super = {0};		// 超级块，注意需初始化
struct proc_struct * super_wait = NULL;

void read_super(void)
{
	struct buffer_head * bh = bread(0, 1);
	memcpy(&super, bh->p_data, sizeof(struct d_super_block));
	brelse(bh);
}

void write_super(void)
{
	struct buffer_head * bh = bread(0, 1);
	memcpy(bh->p_data, &super, sizeof(struct d_super_block));
	bh->state |= BUFF_DELAY_W;
	brelse(bh);
}

/*
 * 分配磁盘索引节点。
 */
struct m_inode * ialloc(unsigned char dev)
{
	struct m_inode * p = NULL;

//	d_printf("ialloc begin.\n");

	while(1)
	{
		if(super.state & SUP_LOCK)
		{
			sleep_on(&super_wait, INTERRUPTIBLE);
			continue;
		}
		/* 磁盘中无空闲索引节点 */
		if(super.nfreeinodes <= 0)
		{
			printf("Kernel MSG: ialloc: have no free-disk-inode.\n");
			return (struct m_inode *)NULL;
		}
		/* 超级块中索引节点表空但还有空闲索引节点，则从铭记节点开始搜索，直到再次搜索到该铭记节点为止 */
		if(super.nextfi >= NR_D_FIT)
		{
			LOCK_SUPER(&super);
			super.nextfi = 0;
			unsigned long nexti = super.store_nexti;
			unsigned long nrblock = INODE_TO_BLOCK(nexti);
			struct buffer_head * bh = bread(0, nrblock);
			int i = super.nextfi;
			for(; i < NR_D_FIT; )
			{
				if(nrblock != INODE_TO_BLOCK(nexti))
				{
					nrblock = INODE_TO_BLOCK(nexti);
					brelse(bh);
					bh = bread(0, nrblock);
				}
				if(0 == ((struct d_inode *)&bh->p_data[INODE_START_IN_BLOCK(nexti)])->nlinks)
				{
					super.fi_table[i++] = nexti;
				}
				(++nexti >= super.ninodes) ? nexti=0 : 0;
				if(nexti == super.store_nexti)
				{
					break;
				}
			}
			brelse(bh);
			super.store_nexti = nexti;
			// 没有找到空闲索引节点，但super.nfreeinodes > 0，所以出现这种情况说明，文件系统出错。在此用于调试。
			if(i == super.nextfi)
			{
				panic("ialloc: FS-free-inode-count is not sync.");
			}
			UNLOCK_SUPER(&super);
		}
		/* 有或有了索引节点 */
		p = iget(0, super.fi_table[super.nextfi++]);
		// NOTE!!! 若该索引节点在内存中已使用，但尚未写入磁盘
		if(p->nlinks > 0)
		{
			// 立即写入磁盘，而不是延迟写
			write_inode(p, 0, p->nrinode, 1);
			iput(p);
			continue;
		}
		--super.nfreeinodes;
		/* 调整索引节点 */
		/*
		 * 以下还未设置
		 * p->create_t = NULL;
		 * p->use_t = NULL;
		 * p->alter_t = NULL;
		 * p->i_alter_t = NULL;
		 */
		p->nlinks = 1;
		p->size = 0;
		zeromem(p->zone, 13);
		/* 将索引节点写入磁盘 */
		write_inode(p, 0, p->nrinode, 0);
		/* 标记超级块已修改 */
		super.state |= SUP_ALT;
		return p;
	}
}

/*
 * 释放磁盘索引节点。
 */
void ifree(unsigned char dev, unsigned int nrinode)
{
//	d_printf("ifree begin.\n");

	struct m_inode tmp_m_inode = {0};
	read_inode(&tmp_m_inode, dev, nrinode);
	/* 如果索引节点连接数不为0，则出错 */
	if(tmp_m_inode.nlinks)
	{
		panic("ifree: trying to free no-free-disk-inode.");
	}

	/* 超级块被锁住 */
	while(super.state & SUP_LOCK)
	{
		sleep_on(&super_wait, INTERRUPTIBLE);
	}
	++super.nfreeinodes;
	/* 如果超级块索引节点表满... */
	if(0 == super.nextfi)
	{
		if(nrinode < super.store_nexti)
		{
			super.store_nexti = nrinode;
		}
	}
	else
	{
		super.fi_table[--super.nextfi] = nrinode;
	}
	/* 标记超级块已修改 */
	super.state |= SUP_ALT;
}

/*
 * 分配磁盘块。
 */
struct buffer_head * alloc(unsigned char dev)
{
	struct buffer_head * bh = NULL;

//	d_printf("alloc begin.\n");

	/* 超级块被锁住 */
	while(super.state & SUP_LOCK)
	{
		sleep_on(&super_wait, INTERRUPTIBLE);
	}
	/* 磁盘中无空闲磁盘块 */
	if(super.nfreeblocks <= 0)
	{
		printf("Kernel MSG: alloc: have no free-disk-block.\n");
		return (struct buffer_head *)NULL;
	}
	unsigned long nrblock = super.fb_table[super.nextfb++];
	/* 如果摘下的是超级块空闲块表中最后一个 */
	if(super.nextfb == NR_D_FBT)
	{
		LOCK_SUPER(&super);
		bh = bread(dev, nrblock);
		memcpy(&super.fb_table, bh->p_data, NR_D_FBT * sizeof(unsigned long));
		brelse(bh);
		super.nextfb = 0;
		UNLOCK_SUPER(&super);
	}
	bh = getblk(dev, nrblock);
	--super.nfreeblocks;
	zeromem(bh->p_data, 512);
	/* 标记超级块已修改 */
	super.state |= SUP_ALT;
	return bh;
}

/*
 * 检查给定偏移地址处的数据块或间接块是否存在，若不存在则分配一个磁盘数据块，
 * 并将数据块链接入索引节点。对于一个索引节点，若间接块不存在则同时也分配间接块。
 *
 * 该程序还是比较费时的，因为它要不断读取磁盘块，而这不是必须的，是可以改进的。
 * 但我还没改进呢 :-)
 *
 * NOTE! 改程序在检查和分配磁盘块时，进行了试探，先调整size字段，以便调用bmap函数，
 * 若没有可用磁盘块，就退回。
 */
BOOL check_alloc_data_block(struct m_inode * p, unsigned long offset)
{
	struct buffer_head * bh = NULL;
	unsigned long savedsize = p->size;		// 保存size字段，以便没有可分配的磁盘块时，恢复size字段
	struct bmap_struct s_bmap = {0};

//	d_printf("check_alloc_data_block begin.\n");

	p->size = (p->size <= offset) ? (offset+1) : p->size;
	bmap(p, offset, &s_bmap);

	if(s_bmap.nrblock)	/* 块已存在 */
	{
		/*
		 * nothing, it's right. NOTE! here, we CAN'T recover the size of i-node.
		 * because, when size > offset, we needn't recover. when size <= offset,
		 * we alose keep it, so that we can call the bmap.
		 */
	}
	else if(0 == s_bmap.step) /* 块不存在，且间接级为0 */
	{
		if(!(bh = alloc(0)))
		{
			/* 如果没有空闲磁盘块，退回 */
			p->size = savedsize;
			return FALSE;
		}
		p->zone[s_bmap.b_index] = bh->nrblock;

//		d_printf("check_alloc_data_block: bh->nrblock=%d, p->zone[s_bmap.b_index]=%d.\n",
//			bh->nrblock, p->zone[s_bmap.b_index]);

		brelse(bh);
	}
	else /* 块不存在，且间接级大于0 */
	{
		unsigned long nrblock = p->zone[s_bmap.step+9];		// 记录当前块的块号
		unsigned long last_nrblock = NULL;					// 记录当前块的上一个块的块号
		unsigned long last_index = s_bmap.step+9;			// 记录当前块在上一个间接级块中的存放位置
		unsigned char step = 1 + s_bmap.step;				// 间接级+1
		/*
		 * NOTE! 以下两个数组变量是指针类型，因为它们所指向的变量值可能会改变。
		 * 关于这两个数组的元素的含义，参考 struct bmap_struct 结构体。
		 */
		unsigned long * b_index[4] = {NULL, &(s_bmap.b_index), &(s_bmap.end_b_index), &(s_bmap.mid_b_index),};
		unsigned long * b_nrblock[4] = {NULL, &(s_bmap.nrblock), &(s_bmap.end_jmp_nrblock), &(s_bmap.mid_jmp_nrblock),};
		while(step--)
		{
			// 如果当前块存在，continue
			if(nrblock)
			{
				last_nrblock = nrblock;
				nrblock = *(b_nrblock[step]);
				last_index = *(b_index[step]);
				continue;
			}
			// 如果当前块不存在
			if(!(bh=alloc(0)))	/* alloc并不读取块数据 */
			{
				p->size = savedsize;
				return FALSE;
			}
			nrblock = bh->nrblock;
			brelse(bh);
			if(step == s_bmap.step)
			{
				p->zone[last_index] = nrblock;
			}
			else
			{
//				d_printf("check_alloc_data_block: last_nrblock=%d.\n", last_nrblock);

				/* 将当前磁盘块号记录在上一级磁盘块上，并延迟写上一级磁盘块 */
				bh = bread(0, last_nrblock);
				*(unsigned long *)&bh->p_data[4 * last_index] = nrblock;
				bh->state |= BUFF_DELAY_W;
				brelse(bh);
			}
			/*
			 * NOTE! 可能情况下每次循环都需要调用，原因...
			 */
			bmap(p, offset, &s_bmap);
			last_nrblock = nrblock;
			nrblock = *(b_nrblock[step]);
			last_index = *(b_index[step]);
		}
	}

	return TRUE;
}

/*
 * 释放磁盘块。
 */
void free(unsigned char dev, unsigned long nrblock)
{
	if(!nrblock)
	{
		panic("free: trying to free NULL-disk-block.");
	}

//	d_printf("free begin.\n");

	/* 超级块被锁住 */
	while(super.state & SUP_LOCK)
	{
		sleep_on(&super_wait, INTERRUPTIBLE);
	}
	/* 如果超级块中空闲块表满... */
	if(0 == super.nextfb)
	{
		LOCK_SUPER(&super);
		struct buffer_head * bh = bread(dev, nrblock);
		memcpy(bh->p_data, &super.fb_table, NR_D_FBT * sizeof(unsigned long));
		bh->state |= BUFF_DELAY_W;
		brelse(bh);
		super.nextfb = NR_D_FBT - 1;	// 修改，使超级块空闲块表中只有唯一一项
		UNLOCK_SUPER(&super);
	}
	super.fb_table[super.nextfb++] = nrblock;
	/* 标记超级块已修改 */
	super.state |= SUP_ALT;	
}

/*
 * 释放文件的所有磁盘块。该函数释放磁盘块(包括数据块和间址块)之后，将索引节点的
 * 直接和间接块指针清0，但并不把索引节点的size字段置为0 !!!
 */
void free_all(struct m_inode * p)
{
	if(!p)
	{
		panic("free_all: the pointer of memory inode can't be NULL.");
	}
	if(!p->mode)
	{
		panic("free_all: inode can't be free-inode.");
	}

//	d_printf("free_all begin.\n");

	struct bmap_struct s_bmap = {0};
	unsigned long last_end_jmp_nrblock = 0;		// 上次的 二次间址或三次间址的最后一个间址块的块号
	unsigned long last_mid_jmp_nrblock = 0;		// 上次的 三次间址中间一个间址块的块号
	unsigned long i=0;
	for(i=0; i<p->size; i+=512)
	{
		if(!bmap(p, i, &s_bmap))
		{
			panic("free_all: wrong happened when free blocks.");			
		}
		/* 释放数据块。文件可能存在空洞，故需判断 */
		if(s_bmap.nrblock)
		{
			free(0, s_bmap.nrblock);
		}
		/* 初始化 last_end_jmp_nrblock */
		if(2 == s_bmap.step && 0 == last_end_jmp_nrblock)
		{
			last_end_jmp_nrblock = s_bmap.end_jmp_nrblock;
		}
		/* 初始化 last_mid_jmp_nrblock */
		if(3 == s_bmap.step && 0 == last_mid_jmp_nrblock)
		{
			last_mid_jmp_nrblock = s_bmap.mid_jmp_nrblock;
		}
		/* 以下依赖于bmap会初始化s_bmap为0 */
		/* 释放上次的 二次间址或三次间址的最后一个间址块 */
		if(last_end_jmp_nrblock && last_end_jmp_nrblock != s_bmap.end_jmp_nrblock)
		{
			free(0, last_end_jmp_nrblock);
			last_end_jmp_nrblock = s_bmap.end_jmp_nrblock;
		}
		/* 释放上次的 三次间址中间一个间址块 */
		if(last_mid_jmp_nrblock && last_mid_jmp_nrblock != s_bmap.mid_jmp_nrblock)
		{
			free(0, last_mid_jmp_nrblock);
			last_mid_jmp_nrblock = s_bmap.mid_jmp_nrblock;
		}
	}
	/* 对上面进行末尾处理 */
	if(last_end_jmp_nrblock)
	{
		free(0, last_end_jmp_nrblock);
	}
	if(last_mid_jmp_nrblock)
	{
		free(0, last_mid_jmp_nrblock);
	}
	/* 释放索引节点直接指向的间址块 */
	for(i=10; i<13; i++)
	{
		if(p->zone[i])
		{
			free(0, p->zone[i]);
		}
	}
	/* 将索引节点直接和间接块指针清0 */
	for(i=0; i<13; i++)
	{
		p->zone[i] = 0;
	}
}

void super_init(void)
{
	super_wait = NULL;

	read_super();
	super.state = 0;
}
