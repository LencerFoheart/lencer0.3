/*
 * Small/fs/buffer.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-02-22 08:44:01
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * ���ļ���������������ش������
 *
 * ����������ʼ��ַ���ں˴�������ݵĽ�������
 * NOTE! ����������ʼ��ַ�ͽ�����ַ����mm�����á�
 *
 * [??] �������л���������͹�ϣ����Ҫ������Ŀǰ��û��!!!
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
 * ��������ʼ�ͽ����������ַ��
 *
 * NOTE! buffer_mem_start, buffer_mem_end����mm�г�ʼ�����ʲ����ڱ��ļ����ٴγ�ʼ��!!!
 */
unsigned long buffer_mem_start = 0, buffer_mem_end = 0;

static struct buffer_head *free_head = NULL;				// ���������б�ͷ��ָ��
static struct buffer_head *hash_table[NR_BUFF_HASH] = {0};	// ��������ϣ��ָ������

struct proc_struct * buff_wait = NULL;						// ָ��ȴ��������Ľ���


/*
 * �ú���Ϊ��ϣ��ɢ�к����������豸�źͿ�ţ��������ڹ�ϣ�������е�������
 * NOTE! �ú�������֪���ÿ��Ƿ������ɢ�б�
 */
static int hash_fun(unsigned char dev, unsigned long nrblock)
{
	return (nrblock % NR_BUFF_HASH);
}

/*
 * �ú�������ϣ����ָ��������ڹ�ϣ���򷵻�ָ��û�������ָ��,���򣬷���NULL.
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
 * �ú����ӿ��б���ժȡһ�����л���������������NULL�����ͷ��ժȡһ����������������pֵժȡ��
 */
static struct buffer_head * get_from_free(struct buffer_head *p)
{
	if(p)						// �ӱ���ժ��ָ���Ŀ��л�����
	{
		if(p->p_next_free == p)	// ����ֻ��һ��������
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
			p = (struct buffer_head *)NULL;
		}
		else if(free_head->p_next_free == free_head)	// ����ֻ��һ��������
		{
			p = free_head;
			free_head = NULL;
		}
		else					// �����ж��������
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
 * �ú��������л�����������б��С�ע�⣺��������Ƿ�����󣬵���������ʱ��������ǰ�档
 */
static void put_to_free(struct buffer_head *p, BOOL ishead)
{
	if(!p)
	{
		return;
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
static void remove_from_hash(struct buffer_head *p)
{
	if(!p || (p && !scan_hash(p->dev, p->nrblock)))
	{
		return;
	}

	int index = hash_fun(p->dev, p->nrblock);

	p->dev = p->nrblock = NULL;
	if(p->p_hash_next == p)	// ����ֻ��һ��������
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
		if(p = scan_hash(dev, nrblock))/* ��ɢ�б� */
		{
			/* ��ɢ�б������� */
			if(p->state & BUFF_LOCK)
			{
				sleep_on(&buff_wait, INTERRUPTIBLE);
				continue;		// �ص�whileѭ��
			}
			/* ��ɢ�б��ҿ��� */
			LOCK_BUFF(p);
			get_from_free(p);
			return p;
		}
		else/* ����ɢ�б� */
		{
			/* ����ɢ�б��ҿ��б�� */
			if(NULL == free_head)
			{
				sleep_on(&buff_wait, INTERRUPTIBLE);
				continue;		// �ص�whileѭ��			
			}
			/* ����ɢ�б��ڿ��б��ҵ�һ������������������ӳ�д */
			p = get_from_free((struct buffer_head *)NULL);
			if((p->state & BUFF_DELAY_W) && !(p->state & BUFF_LOCK))
			{
				/* ��������д����� */
				LOCK_BUFF(p);
				bwrite(p);
				continue;		// �ص�whileѭ��
			}
			/* ����ɢ�б��ڿ��б��ҵ�һ�������� */
			LOCK_BUFF(p);						// ����
			p->state &= (~BUFF_VAL) & 0xff;		// ʹ��Ч
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
	if(!(p->state & BUFF_VAL))	// ��Ч�����������̶�
	{
		/* �������̶� */
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

	/* ��������д */
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
 * NOTE! buff_init() ���� mem_init() ֮�󱻵��á�
 */
void buff_init(void)
{
	int buffers = 0;						// ��������
	unsigned long tmp_next_free = 0;		// ָ����һ����������������ʱ�ı����Դ�ʩ
	struct buffer_head *p = (struct buffer_head *)buffer_mem_start;	// ָ��ǰҪ��������
	struct buffer_head *plast = NULL;		// ǰһ��������ͷ��ָ��

	/* NOTE! ����1��Ϊ�˱��⣬��640kb���޷��洢�����Ļ���ͷ���򻺳��������� */
	buffers = (buffer_mem_end - buffer_mem_start - (1*1024*1024-640*1024)) / (BUFFER_SIZE + sizeof(struct buffer_head)) - 1;
	free_head = p;
	buff_wait = NULL;

//	d_printf("buff_head+BUFFER_SIZE = %d\n buffers = %d\n", sizeof(struct buffer_head)+BUFFER_SIZE, buffers);

	// ����˫��ѭ�����л���������
	while(buffers--)
	{
		/*
		 * NOTE! ��������һ�ֿ����Դ��󣺵� p δ�ﵽ640KB���� p->p_next_free �ѳ����ˣ�
		 * ���Դ˴�������ʹ�� p->p_next_free�����ǲ�����ʱ����ȥ��̽��
		 */
		tmp_next_free = (unsigned long)p + sizeof(struct buffer_head) + BUFFER_SIZE;
		// ��ǰ����������[640KB,1MB]����
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

	// ��ʼ����ϣ����
	for(int i=0; i<NR_BUFF_HASH; i++)
	{
		hash_table[i] = NULL;
	}
}