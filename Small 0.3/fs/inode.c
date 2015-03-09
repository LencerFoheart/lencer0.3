/*
 * Small/fs/inode.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-2-28 16:09:28
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * 此文件为内存inode处理文件。内存inode的处理方式与buffer的相似，都采用了空闲表和哈希表。
 * 故可用相似的空闲表和哈希表操作，此文件与buffer.c中的对空闲表和哈希表的处理程序雷同!
 * 其实我是从buffer.c直接复制过来的 :-)
 *
 * [??] 索引节点是否可延迟写，尚需思考!!!
 * [??] 整个空闲索引节点链表和哈希表需要加锁，目前还没有!!!
 */

//#ifndef __DEBUG__
//#define __DEBUG__
//#endif

#include "file_system.h"
#include "types.h"
#include "string.h"
#include "sched.h"
#include "kernel.h"
#include "stdio.h"

static struct m_inode inode_table[NR_INODE] = {{0,},};			// inode空闲表
static struct m_inode *free_head = NULL;						// inode空闲表头部指针
static struct m_inode *hash_table[NR_INODE_HASH] = {0};			// inode哈希表指针数组

struct proc_struct * inode_wait = NULL;							// 指向等待inode的进程


/*
 * 该函数为哈希表散列函数。根据设备号和索引节点号，返回其在哈希表数组中的索引。
 * NOTE! 该函数并不知道该索引节点是否存在于散列表。
 */
static int hash_fun(unsigned char dev, unsigned long nrinode)
{
	return (nrinode % NR_INODE_HASH);
}

/*
 * 该函数检查哈希表，若指定索引节点存在于哈希表，则返回指向该inode的指针,否则，返回NULL.
 */
static struct m_inode * scan_hash(unsigned char dev, unsigned long nrinode)
{
	int index = hash_fun(dev, nrinode);
	struct m_inode *p = hash_table[index];

	while(p)
	{
		if(p->dev == dev && p->nrinode == nrinode)
		{
			return p;
		}
		p = p->p_hash_next;
		if(p == hash_table[index])
		{
			break;
		}
	}
	return (struct m_inode *)NULL;
}

/*
 * 该函数从空闲表中摘取一个空闲inode。输入若是NULL，则从头部摘取一个，否则根据输入的p值摘取。
 */
static struct m_inode * get_from_free(struct m_inode *p)
{
	if(p)						// 从表中摘下指定的空闲inode
	{
		if(p->p_next_free == p)	// 表中只有一个inode
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
			p = (struct m_inode *)NULL;
		}
		else if(free_head->p_next_free == free_head)	// 表中只有一个inode
		{
			p = free_head;
			free_head = NULL;
		}
		else					// 表中有多个inode
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
 * 该函数将空闲inode放入空闲表中。注意：正常情况是放入最后，当发生错误时，放入最前面。
 */
static void put_to_free(struct m_inode *p, BOOL ishead)
{
	if(!p)
	{
		return;
	}

	if(p->count)
	{
		printf("put_to_free: p->count = %d.\n", p->count);
		panic("put_to_free: trying to free the not-free-inode.");
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
static void remove_from_hash(struct m_inode *p)
{
	if(!p || (p && !scan_hash(p->dev, p->nrinode)))
	{
		return;
	}

	int index = hash_fun(p->dev, p->nrinode);

	p->dev = p->nrinode = NULL;
	if(p->p_hash_next == p)	// 表中只有一个inode
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
static void put_to_hash(struct m_inode *p, unsigned char newdev, unsigned long newnrinode)
{
	if(!p)
	{
		return;
	}

	int index = hash_fun(newdev, newnrinode);

	p->dev = newdev;
	p->nrinode = newnrinode;
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

void read_inode(struct m_inode * p, unsigned char dev, unsigned int nrinode)
{
	if(!p)
	{
		panic("read_inode: the pointer of memory inode can't be NULL.");
	}

//	d_printf("read_inode begin.\n");

	struct buffer_head * bh = bread(dev, INODE_TO_BLOCK(nrinode));
	memcpy(p, &bh->p_data[INODE_START_IN_BLOCK(nrinode)], sizeof(struct d_inode));
	brelse(bh);
}

/*
 * 参数isnow指示是否立马写入磁盘。
 */
void write_inode(struct m_inode * p, unsigned char dev, unsigned int nrinode, char isnow)
{
	if(!p)
	{
		panic("write_inode: the pointer of memory inode can't be NULL.");
	}

	struct buffer_head * bh = bread(dev, INODE_TO_BLOCK(p->nrinode));
	memcpy(&bh->p_data[INODE_START_IN_BLOCK(nrinode)], p, sizeof(struct d_inode));
	if(!isnow)
	{
		bh->state |= BUFF_DELAY_W;
		brelse(bh);
	}
	else
	{
		/*
		 * 立即写，我们需复位延迟写标志，即使没有此标志。
		 */
		bh->state &= (~BUFF_DELAY_W) & 0xff;
		bwrite(bh);
	}
}

struct m_inode * iget(unsigned char dev, unsigned int nrinode)
{
	struct m_inode * p =NULL;

//	d_printf("iget begin.\n");

	while(1)
	{
		if(p = scan_hash(dev, nrinode))/* 在散列表 */
		{
			/* 在散列表，但上锁 */
			if(p->state & NODE_LOCK)
			{
				sleep_on(&inode_wait, INTERRUPTIBLE);
				continue;		// 回到while循环
			}
			LOCK_INODE(p);
			if(!(p->count++))	/* 在散列表，且在空闲表 */
			{
				get_from_free(p);
			}
			return p;
		}
		else/* 不在散列表 */
		{
			/* 不在散列表，且空闲表空 */
			if(NULL == free_head)
			{
				sleep_on(&inode_wait, INTERRUPTIBLE);
				continue;		// 回到while循环			
			}
			p = get_from_free((struct m_inode *)NULL);
			/* 不在散列表，在空闲表找到一个inode */
			LOCK_INODE(p);						// 上锁
			p->state &= (~NODE_VAL) & 0xff;		// 使无效
			remove_from_hash(p);
			put_to_hash(p, dev, nrinode);
			/* 读取磁盘索引节点 */
			read_inode(p, 0, nrinode);
			/* 调整内存索引节点 */
			p->state |= NODE_VAL;
			p->count = 1;
			return p;
		}
	}
}

void iput(struct m_inode * p)
{
//	d_printf("iput begin.\n");

	if(!(p->state & NODE_LOCK))
	{
		LOCK_INODE(p);
	}
	if(0 == (--p->count))
	{
		printf("iput: p->count = %d.\n", p->count);
		if(0 == p->nlinks)	// 索引节点连接数为0，释放文件磁盘块，释放索引节点
		{
			free_all(p);
			ifree(p->dev, p->nrinode);
		}
		// 此处也可以不判断文件数据是否修改，因为数据修改，索引节点必定修改
		if((p->state & NODE_I_ALT) || (p->state & NODE_D_ALT))
		{
			write_inode(p, 0, p->nrinode, 0);
		}
		put_to_free(p, 0);
	}
	UNLOCK_INODE(p);
}

/*
 * 文件逻辑字节偏移量到文件磁盘块的映射。若逻辑字节超过文件大小，返回FALSE；
 * 否则，映射结果记录在结构体中。
 */
BOOL bmap(struct m_inode * p, unsigned long v_offset, struct bmap_struct * s_bmap)
{
//	d_printf("bmap begin.\n");

	if(v_offset >= p->size)
	{
		printf("Kernel MSG: bmap: the byte-offset out of limit.\n");
		return FALSE;
	}
	s_bmap->offset = v_offset % 512;
	s_bmap->bytes = ((p->size-1) % 512) + 1;
	unsigned long v_nrblock = v_offset / 512;
	char step = 0;		// 间接级
	s_bmap->nrblock = p->zone[v_nrblock % 10];
	s_bmap->b_index = v_nrblock % 10;
	if(v_nrblock > (9+512/4+512/4*512/4))
	{
		step = 3;
		s_bmap->nrblock = p->zone[12];
		v_nrblock -= 10 + 512/4 + 512/4*512/4;
	}
	else if(v_nrblock > (9+512/4))
	{
		step = 2;
		s_bmap->nrblock = p->zone[11];
		v_nrblock -= 10 + 512/4;
	}
	else if(v_nrblock > 9)	// 10个直接块
	{
		step = 1;
		s_bmap->nrblock = p->zone[10];
		v_nrblock -= 10;
	}
	s_bmap->step = step;
	s_bmap->mid_jmp_nrblock = 0;
	s_bmap->mid_b_index = 0;			
	s_bmap->end_jmp_nrblock = 0;
	s_bmap->end_b_index = 0;
	while(step--)
	{
		/* 判断可能的文件空洞 */
		if(!s_bmap->nrblock)
		{
			return TRUE;
		}

		/*
		 * Well, it's LONG, not CHAR !!! As this fault, I debug for many days. :-(((
		 * But, it seems having other fault.
		 */
		unsigned long tmp = 1;
		for(int i=0; i<step; i++)
		{
			tmp *= 512/4;
		}
		unsigned long tmp_index = v_nrblock / tmp;
		v_nrblock %= tmp;
		struct buffer_head * bh = bread(0, s_bmap->nrblock);
		s_bmap->nrblock = *(unsigned long *)&bh->p_data[4 * tmp_index];
		brelse(bh);
		if(2 == step)
		{
			s_bmap->mid_jmp_nrblock = s_bmap->nrblock;
			s_bmap->mid_b_index = tmp_index;			
		}
		else if(1 == step)
		{
			s_bmap->end_jmp_nrblock = s_bmap->nrblock;
			s_bmap->end_b_index = tmp_index;
		}
		else
		{
			s_bmap->b_index = tmp_index;
		}
	}

	return TRUE;
}

/*
 * 路径名到索引节点的转换。
 */
struct m_inode * namei(char * name)
{
	char * pstr = name;
	struct m_inode * dir = NULL;
	char path_part[MAX_PATH_PART+1] = {0};
	int i = 0;

//	d_printf("namei begin.\n");

	if(NULL == pstr)
	{
		printf("Kernel MSG: namei: name can't be NULL\n");
		return (struct m_inode *)NULL;
	}

	if('/' == *pstr)/* 根目录 */
	{
		dir = iget(current->root_dir->dev, current->root_dir->nrinode);
		++pstr;
	}
	else/* 当前目录 */
	{
		dir = iget(current->current_dir->dev, current->current_dir->nrinode);
	}
	while(*pstr)
	{
		i = 0;
		/* 获取一个路径分量 */
		while('/' != *pstr && *pstr)
		{
			path_part[i++] = *(pstr++);
		}
		path_part[i] = 0;
		if(*pstr)
		{
			++pstr;
		}
		// 不是目录
		if(!(dir->mode & FILE_DIR))
		{
			goto namei_false_ret;
		}
		// 根目录无父目录
		if(dir==current->root_dir && 0==strcmp(path_part,".."))
		{
			goto namei_false_ret;
		}
		// 读取目录文件，逐个目录项比较
		unsigned long offset = 0;
		struct bmap_struct s_bmap = {0};
		struct buffer_head * bh = NULL;
		while(offset < dir->size)
		{
			if(bmap(dir, offset, &s_bmap))
			{
				if(s_bmap.nrblock)	/* 块存在 */
				{
					bh = bread(0, s_bmap.nrblock);
//					d_printf("namei: s_bmap.nrblock = %d.\n", s_bmap.nrblock);
					/*
					 * NOTE! 此处依赖于写目录的方式。本版内核默认是一块存储整数个目录项。
					 */
					while(s_bmap.offset + sizeof(struct file_dir_struct) <= s_bmap.bytes)
					{
						// 相同，则跳出
						if(0 == strcmp(path_part,
							((struct file_dir_struct *)&bh->p_data[s_bmap.offset])->name))
						{
							goto namei_out;
						}
						s_bmap.offset += sizeof(struct file_dir_struct);
					}
					brelse(bh);
				}
			}
			offset = (offset / 512 + 1) * 512;
		}

namei_out:

		if(offset < dir->size)	/* 如果找到 */
		{
			iput(dir);
			dir = iget(0, ((struct file_dir_struct *)&bh->p_data[s_bmap.offset])->nrinode);
			brelse(bh);			// goto没有释放
		}
		else	/* 工作目录中无此分量 */
		{
			goto namei_false_ret;
		}
	}

	return dir;		/* TRUE ret */

namei_false_ret:	/* FALSE ret */

	iput(dir);			// NOTE! we need iput.
	return (struct m_inode *)NULL;
}

void inode_init(void)
{
	int inodes = NR_INODE;					// inode数
	struct m_inode * p = &inode_table[0];	// 指向指向当前要处理inode
	struct m_inode * plast = NULL;			// 前一个inode头部指针

	free_head = p;
	inode_wait = NULL;

	// 建立双向循环空闲inode链表
	while(inodes--)
	{
		zeromem(p, sizeof(struct m_inode));
		p->p_next_free = p + 1;
		p->p_pre_free = plast;

		plast = p;
		p = p->p_next_free;
	}
	plast->p_next_free = free_head;
	free_head->p_pre_free = plast;

	// 初始化哈希链表
	for(int i=0; i<NR_INODE_HASH; i++)
	{
		hash_table[i] = NULL;
	}
}
