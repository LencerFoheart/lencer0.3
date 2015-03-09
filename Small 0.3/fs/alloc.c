/*
 * Small/fs/alloc.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-3-2 18:54:22
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * ���ļ��������������ڵ�ʹ��̿�ķ����Լ��ͷš�
 *
 * [??] ������Ӧ����д����̣�Ŀǰ��ûʵ�ָù��ܡ�
 * [??] �������Ƿ���ӳ�д������˼��!!!
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

struct m_super_block super = {0};		// �����飬ע�����ʼ��
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
 * ������������ڵ㡣
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
		/* �������޿��������ڵ� */
		if(super.nfreeinodes <= 0)
		{
			printf("Kernel MSG: ialloc: have no free-disk-inode.\n");
			return (struct m_inode *)NULL;
		}
		/* �������������ڵ��յ����п��������ڵ㣬������ǽڵ㿪ʼ������ֱ���ٴ������������ǽڵ�Ϊֹ */
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
			// û���ҵ����������ڵ㣬��super.nfreeinodes > 0�����Գ����������˵�����ļ�ϵͳ�����ڴ����ڵ��ԡ�
			if(i == super.nextfi)
			{
				panic("ialloc: FS-free-inode-count is not sync.");
			}
			UNLOCK_SUPER(&super);
		}
		/* �л����������ڵ� */
		p = iget(0, super.fi_table[super.nextfi++]);
		// NOTE!!! ���������ڵ����ڴ�����ʹ�ã�����δд�����
		if(p->nlinks > 0)
		{
			// ����д����̣��������ӳ�д
			write_inode(p, 0, p->nrinode, 1);
			iput(p);
			continue;
		}
		--super.nfreeinodes;
		/* ���������ڵ� */
		/*
		 * ���»�δ����
		 * p->create_t = NULL;
		 * p->use_t = NULL;
		 * p->alter_t = NULL;
		 * p->i_alter_t = NULL;
		 */
		p->nlinks = 1;
		p->size = 0;
		zeromem(p->zone, 13);
		/* �������ڵ�д����� */
		write_inode(p, 0, p->nrinode, 0);
		/* ��ǳ��������޸� */
		super.state |= SUP_ALT;
		return p;
	}
}

/*
 * �ͷŴ��������ڵ㡣
 */
void ifree(unsigned char dev, unsigned int nrinode)
{
//	d_printf("ifree begin.\n");

	struct m_inode tmp_m_inode = {0};
	read_inode(&tmp_m_inode, dev, nrinode);
	/* ��������ڵ���������Ϊ0������� */
	if(tmp_m_inode.nlinks)
	{
		panic("ifree: trying to free no-free-disk-inode.");
	}

	/* �����鱻��ס */
	while(super.state & SUP_LOCK)
	{
		sleep_on(&super_wait, INTERRUPTIBLE);
	}
	++super.nfreeinodes;
	/* ��������������ڵ����... */
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
	/* ��ǳ��������޸� */
	super.state |= SUP_ALT;
}

/*
 * ������̿顣
 */
struct buffer_head * alloc(unsigned char dev)
{
	struct buffer_head * bh = NULL;

//	d_printf("alloc begin.\n");

	/* �����鱻��ס */
	while(super.state & SUP_LOCK)
	{
		sleep_on(&super_wait, INTERRUPTIBLE);
	}
	/* �������޿��д��̿� */
	if(super.nfreeblocks <= 0)
	{
		printf("Kernel MSG: alloc: have no free-disk-block.\n");
		return (struct buffer_head *)NULL;
	}
	unsigned long nrblock = super.fb_table[super.nextfb++];
	/* ���ժ�µ��ǳ�������п�������һ�� */
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
	/* ��ǳ��������޸� */
	super.state |= SUP_ALT;
	return bh;
}

/*
 * ������ƫ�Ƶ�ַ�������ݿ���ӿ��Ƿ���ڣ��������������һ���������ݿ飬
 * �������ݿ������������ڵ㡣����һ�������ڵ㣬����ӿ鲻������ͬʱҲ�����ӿ顣
 *
 * �ó����ǱȽϷ�ʱ�ģ���Ϊ��Ҫ���϶�ȡ���̿飬���ⲻ�Ǳ���ģ��ǿ��ԸĽ��ġ�
 * ���һ�û�Ľ��� :-)
 *
 * NOTE! �ĳ����ڼ��ͷ�����̿�ʱ����������̽���ȵ���size�ֶΣ��Ա����bmap������
 * ��û�п��ô��̿飬���˻ء�
 */
BOOL check_alloc_data_block(struct m_inode * p, unsigned long offset)
{
	struct buffer_head * bh = NULL;
	unsigned long savedsize = p->size;		// ����size�ֶΣ��Ա�û�пɷ���Ĵ��̿�ʱ���ָ�size�ֶ�
	struct bmap_struct s_bmap = {0};

//	d_printf("check_alloc_data_block begin.\n");

	p->size = (p->size <= offset) ? (offset+1) : p->size;
	bmap(p, offset, &s_bmap);

	if(s_bmap.nrblock)	/* ���Ѵ��� */
	{
		/*
		 * nothing, it's right. NOTE! here, we CAN'T recover the size of i-node.
		 * because, when size > offset, we needn't recover. when size <= offset,
		 * we alose keep it, so that we can call the bmap.
		 */
	}
	else if(0 == s_bmap.step) /* �鲻���ڣ��Ҽ�Ӽ�Ϊ0 */
	{
		if(!(bh = alloc(0)))
		{
			/* ���û�п��д��̿飬�˻� */
			p->size = savedsize;
			return FALSE;
		}
		p->zone[s_bmap.b_index] = bh->nrblock;

//		d_printf("check_alloc_data_block: bh->nrblock=%d, p->zone[s_bmap.b_index]=%d.\n",
//			bh->nrblock, p->zone[s_bmap.b_index]);

		brelse(bh);
	}
	else /* �鲻���ڣ��Ҽ�Ӽ�����0 */
	{
		unsigned long nrblock = p->zone[s_bmap.step+9];		// ��¼��ǰ��Ŀ��
		unsigned long last_nrblock = NULL;					// ��¼��ǰ�����һ����Ŀ��
		unsigned long last_index = s_bmap.step+9;			// ��¼��ǰ������һ����Ӽ����еĴ��λ��
		unsigned char step = 1 + s_bmap.step;				// ��Ӽ�+1
		/*
		 * NOTE! �����������������ָ�����ͣ���Ϊ������ָ��ı���ֵ���ܻ�ı䡣
		 * ���������������Ԫ�صĺ��壬�ο� struct bmap_struct �ṹ�塣
		 */
		unsigned long * b_index[4] = {NULL, &(s_bmap.b_index), &(s_bmap.end_b_index), &(s_bmap.mid_b_index),};
		unsigned long * b_nrblock[4] = {NULL, &(s_bmap.nrblock), &(s_bmap.end_jmp_nrblock), &(s_bmap.mid_jmp_nrblock),};
		while(step--)
		{
			// �����ǰ����ڣ�continue
			if(nrblock)
			{
				last_nrblock = nrblock;
				nrblock = *(b_nrblock[step]);
				last_index = *(b_index[step]);
				continue;
			}
			// �����ǰ�鲻����
			if(!(bh=alloc(0)))	/* alloc������ȡ������ */
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

				/* ����ǰ���̿�ż�¼����һ�����̿��ϣ����ӳ�д��һ�����̿� */
				bh = bread(0, last_nrblock);
				*(unsigned long *)&bh->p_data[4 * last_index] = nrblock;
				bh->state |= BUFF_DELAY_W;
				brelse(bh);
			}
			/*
			 * NOTE! ���������ÿ��ѭ������Ҫ���ã�ԭ��...
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
 * �ͷŴ��̿顣
 */
void free(unsigned char dev, unsigned long nrblock)
{
	if(!nrblock)
	{
		panic("free: trying to free NULL-disk-block.");
	}

//	d_printf("free begin.\n");

	/* �����鱻��ס */
	while(super.state & SUP_LOCK)
	{
		sleep_on(&super_wait, INTERRUPTIBLE);
	}
	/* ����������п��п����... */
	if(0 == super.nextfb)
	{
		LOCK_SUPER(&super);
		struct buffer_head * bh = bread(dev, nrblock);
		memcpy(bh->p_data, &super.fb_table, NR_D_FBT * sizeof(unsigned long));
		bh->state |= BUFF_DELAY_W;
		brelse(bh);
		super.nextfb = NR_D_FBT - 1;	// �޸ģ�ʹ��������п����ֻ��Ψһһ��
		UNLOCK_SUPER(&super);
	}
	super.fb_table[super.nextfb++] = nrblock;
	/* ��ǳ��������޸� */
	super.state |= SUP_ALT;	
}

/*
 * �ͷ��ļ������д��̿顣�ú����ͷŴ��̿�(�������ݿ�ͼ�ַ��)֮�󣬽������ڵ��
 * ֱ�Ӻͼ�ӿ�ָ����0���������������ڵ��size�ֶ���Ϊ0 !!!
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
	unsigned long last_end_jmp_nrblock = 0;		// �ϴε� ���μ�ַ�����μ�ַ�����һ����ַ��Ŀ��
	unsigned long last_mid_jmp_nrblock = 0;		// �ϴε� ���μ�ַ�м�һ����ַ��Ŀ��
	unsigned long i=0;
	for(i=0; i<p->size; i+=512)
	{
		if(!bmap(p, i, &s_bmap))
		{
			panic("free_all: wrong happened when free blocks.");			
		}
		/* �ͷ����ݿ顣�ļ����ܴ��ڿն��������ж� */
		if(s_bmap.nrblock)
		{
			free(0, s_bmap.nrblock);
		}
		/* ��ʼ�� last_end_jmp_nrblock */
		if(2 == s_bmap.step && 0 == last_end_jmp_nrblock)
		{
			last_end_jmp_nrblock = s_bmap.end_jmp_nrblock;
		}
		/* ��ʼ�� last_mid_jmp_nrblock */
		if(3 == s_bmap.step && 0 == last_mid_jmp_nrblock)
		{
			last_mid_jmp_nrblock = s_bmap.mid_jmp_nrblock;
		}
		/* ����������bmap���ʼ��s_bmapΪ0 */
		/* �ͷ��ϴε� ���μ�ַ�����μ�ַ�����һ����ַ�� */
		if(last_end_jmp_nrblock && last_end_jmp_nrblock != s_bmap.end_jmp_nrblock)
		{
			free(0, last_end_jmp_nrblock);
			last_end_jmp_nrblock = s_bmap.end_jmp_nrblock;
		}
		/* �ͷ��ϴε� ���μ�ַ�м�һ����ַ�� */
		if(last_mid_jmp_nrblock && last_mid_jmp_nrblock != s_bmap.mid_jmp_nrblock)
		{
			free(0, last_mid_jmp_nrblock);
			last_mid_jmp_nrblock = s_bmap.mid_jmp_nrblock;
		}
	}
	/* ���������ĩβ���� */
	if(last_end_jmp_nrblock)
	{
		free(0, last_end_jmp_nrblock);
	}
	if(last_mid_jmp_nrblock)
	{
		free(0, last_mid_jmp_nrblock);
	}
	/* �ͷ������ڵ�ֱ��ָ��ļ�ַ�� */
	for(i=10; i<13; i++)
	{
		if(p->zone[i])
		{
			free(0, p->zone[i]);
		}
	}
	/* �������ڵ�ֱ�Ӻͼ�ӿ�ָ����0 */
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
