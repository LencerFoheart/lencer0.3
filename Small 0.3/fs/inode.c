/*
 * Small/fs/inode.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-2-28 16:09:28
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * ���ļ�Ϊ�ڴ�inode�����ļ����ڴ�inode�Ĵ���ʽ��buffer�����ƣ��������˿��б�͹�ϣ��
 * �ʿ������ƵĿ��б�͹�ϣ����������ļ���buffer.c�еĶԿ��б�͹�ϣ��Ĵ��������ͬ!
 * ��ʵ���Ǵ�buffer.cֱ�Ӹ��ƹ����� :-)
 *
 * [??] �����ڵ��Ƿ���ӳ�д������˼��!!!
 * [??] �������������ڵ�����͹�ϣ����Ҫ������Ŀǰ��û��!!!
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

static struct m_inode inode_table[NR_INODE] = {{0,},};			// inode���б�
static struct m_inode *free_head = NULL;						// inode���б�ͷ��ָ��
static struct m_inode *hash_table[NR_INODE_HASH] = {0};			// inode��ϣ��ָ������

struct proc_struct * inode_wait = NULL;							// ָ��ȴ�inode�Ľ���


/*
 * �ú���Ϊ��ϣ��ɢ�к����������豸�ź������ڵ�ţ��������ڹ�ϣ�������е�������
 * NOTE! �ú�������֪���������ڵ��Ƿ������ɢ�б�
 */
static int hash_fun(unsigned char dev, unsigned long nrinode)
{
	return (nrinode % NR_INODE_HASH);
}

/*
 * �ú�������ϣ����ָ�������ڵ�����ڹ�ϣ���򷵻�ָ���inode��ָ��,���򣬷���NULL.
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
 * �ú����ӿ��б���ժȡһ������inode����������NULL�����ͷ��ժȡһ����������������pֵժȡ��
 */
static struct m_inode * get_from_free(struct m_inode *p)
{
	if(p)						// �ӱ���ժ��ָ���Ŀ���inode
	{
		if(p->p_next_free == p)	// ����ֻ��һ��inode
		{
			free_head = NULL;
		}
		else
		{
			(p->p_pre_free)->p_next_free = p->p_next_free;
			(p->p_next_free)->p_pre_free = p->p_pre_free;
			if(p == free_head)		// pָ���ͷ
			{
				free_head = free_head->p_next_free;
			}
		}
	}
	else
	{
		if(NULL == free_head)	// ���
		{
			p = (struct m_inode *)NULL;
		}
		else if(free_head->p_next_free == free_head)	// ����ֻ��һ��inode
		{
			p = free_head;
			free_head = NULL;
		}
		else					// �����ж��inode
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
 * �ú���������inode������б��С�ע�⣺��������Ƿ�����󣬵���������ʱ��������ǰ�档
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

	if(NULL == free_head)	// ���б��
	{
		free_head = p;
		p->p_next_free = p;
		p->p_pre_free = p;
	}
	else
	{
		if(ishead)			// ����ͷ��
		{
			p->p_next_free = free_head;
			p->p_pre_free = free_head->p_pre_free;
			(free_head->p_pre_free)->p_next_free = p;
			free_head->p_pre_free = p;
			free_head = p;
		}
		else				// ����ĩβ
		{
			p->p_pre_free = free_head->p_pre_free;
			(free_head->p_pre_free)->p_next_free = p;
			p->p_next_free = free_head;
			free_head->p_pre_free = p;
		}
	}
}

/* �ӹ�ϣ���Ƴ� */
static void remove_from_hash(struct m_inode *p)
{
	if(!p || (p && !scan_hash(p->dev, p->nrinode)))
	{
		return;
	}

	int index = hash_fun(p->dev, p->nrinode);

	p->dev = p->nrinode = NULL;
	if(p->p_hash_next == p)	// ����ֻ��һ��inode
	{
		hash_table[index] = NULL;
	}
	else
	{
		(p->p_hash_pre)->p_hash_next = p->p_hash_next;
		(p->p_hash_next)->p_hash_pre = p->p_hash_pre;
		if(p == hash_table[index])		// pָ���ͷ
		{
			hash_table[index] = hash_table[index]->p_hash_next;
		}
	}
}

/* �����ϣ�����ͷ�� */
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
 * ����isnowָʾ�Ƿ�����д����̡�
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
		 * ����д�������踴λ�ӳ�д��־����ʹû�д˱�־��
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
		if(p = scan_hash(dev, nrinode))/* ��ɢ�б� */
		{
			/* ��ɢ�б������� */
			if(p->state & NODE_LOCK)
			{
				sleep_on(&inode_wait, INTERRUPTIBLE);
				continue;		// �ص�whileѭ��
			}
			LOCK_INODE(p);
			if(!(p->count++))	/* ��ɢ�б����ڿ��б� */
			{
				get_from_free(p);
			}
			return p;
		}
		else/* ����ɢ�б� */
		{
			/* ����ɢ�б��ҿ��б�� */
			if(NULL == free_head)
			{
				sleep_on(&inode_wait, INTERRUPTIBLE);
				continue;		// �ص�whileѭ��			
			}
			p = get_from_free((struct m_inode *)NULL);
			/* ����ɢ�б��ڿ��б��ҵ�һ��inode */
			LOCK_INODE(p);						// ����
			p->state &= (~NODE_VAL) & 0xff;		// ʹ��Ч
			remove_from_hash(p);
			put_to_hash(p, dev, nrinode);
			/* ��ȡ���������ڵ� */
			read_inode(p, 0, nrinode);
			/* �����ڴ������ڵ� */
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
		if(0 == p->nlinks)	// �����ڵ�������Ϊ0���ͷ��ļ����̿飬�ͷ������ڵ�
		{
			free_all(p);
			ifree(p->dev, p->nrinode);
		}
		// �˴�Ҳ���Բ��ж��ļ������Ƿ��޸ģ���Ϊ�����޸ģ������ڵ�ض��޸�
		if((p->state & NODE_I_ALT) || (p->state & NODE_D_ALT))
		{
			write_inode(p, 0, p->nrinode, 0);
		}
		put_to_free(p, 0);
	}
	UNLOCK_INODE(p);
}

/*
 * �ļ��߼��ֽ�ƫ�������ļ����̿��ӳ�䡣���߼��ֽڳ����ļ���С������FALSE��
 * ����ӳ������¼�ڽṹ���С�
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
	char step = 0;		// ��Ӽ�
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
	else if(v_nrblock > 9)	// 10��ֱ�ӿ�
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
		/* �жϿ��ܵ��ļ��ն� */
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
 * ·�����������ڵ��ת����
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

	if('/' == *pstr)/* ��Ŀ¼ */
	{
		dir = iget(current->root_dir->dev, current->root_dir->nrinode);
		++pstr;
	}
	else/* ��ǰĿ¼ */
	{
		dir = iget(current->current_dir->dev, current->current_dir->nrinode);
	}
	while(*pstr)
	{
		i = 0;
		/* ��ȡһ��·������ */
		while('/' != *pstr && *pstr)
		{
			path_part[i++] = *(pstr++);
		}
		path_part[i] = 0;
		if(*pstr)
		{
			++pstr;
		}
		// ����Ŀ¼
		if(!(dir->mode & FILE_DIR))
		{
			goto namei_false_ret;
		}
		// ��Ŀ¼�޸�Ŀ¼
		if(dir==current->root_dir && 0==strcmp(path_part,".."))
		{
			goto namei_false_ret;
		}
		// ��ȡĿ¼�ļ������Ŀ¼��Ƚ�
		unsigned long offset = 0;
		struct bmap_struct s_bmap = {0};
		struct buffer_head * bh = NULL;
		while(offset < dir->size)
		{
			if(bmap(dir, offset, &s_bmap))
			{
				if(s_bmap.nrblock)	/* ����� */
				{
					bh = bread(0, s_bmap.nrblock);
//					d_printf("namei: s_bmap.nrblock = %d.\n", s_bmap.nrblock);
					/*
					 * NOTE! �˴�������дĿ¼�ķ�ʽ�������ں�Ĭ����һ��洢������Ŀ¼�
					 */
					while(s_bmap.offset + sizeof(struct file_dir_struct) <= s_bmap.bytes)
					{
						// ��ͬ��������
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

		if(offset < dir->size)	/* ����ҵ� */
		{
			iput(dir);
			dir = iget(0, ((struct file_dir_struct *)&bh->p_data[s_bmap.offset])->nrinode);
			brelse(bh);			// gotoû���ͷ�
		}
		else	/* ����Ŀ¼���޴˷��� */
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
	int inodes = NR_INODE;					// inode��
	struct m_inode * p = &inode_table[0];	// ָ��ָ��ǰҪ����inode
	struct m_inode * plast = NULL;			// ǰһ��inodeͷ��ָ��

	free_head = p;
	inode_wait = NULL;

	// ����˫��ѭ������inode����
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

	// ��ʼ����ϣ����
	for(int i=0; i<NR_INODE_HASH; i++)
	{
		hash_table[i] = NULL;
	}
}
