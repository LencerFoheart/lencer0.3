/*
 * Small/fs/read_write.c
 *
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-3-3 12:04:48
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * NOTE! �ļ�ϵͳ��ϵͳ����һ������£�������-1.
 */

//#ifndef __DEBUG__
//#define __DEBUG__
//#endif

#include "file_system.h"
#include "sched.h"
#include "kernel.h"
#include "string.h"
#include "stdio.h"
#include "types.h"

struct file_table_struct file_table[SYS_MAX_FILES] = {{0,},};	// ϵͳ�ļ���

/*
 * ����ļ��������Ƿ�Ϸ����˴�ֻ���˷Ƿ��ģ������Ϊ�Ϸ���
 * NOTE! �ں˼�����ַǷ���������ļ�ΪĿ¼���ͣ��ǲ�����
 * �����Կ�д�ķ�ʽ�򿪵�!!!
 */
BOOL check_open_file_permit(struct m_inode * p, unsigned short mode)
{
	BOOL isok = TRUE;
	switch(mode)
	{
	case O_WONLY:
	case O_RW:
	case O_CW:
	case O_WA:
		if(!(FILE_RW & p->mode) || (p->mode & FILE_DIR))
		{
			isok = FALSE;
		}
		break;
	default:
		/* nothing */
		break;
	}
	return isok;
}

/*
 * ����ϵͳ�ļ����������ʧ�ܣ�����-1�����򷵻�����ֵ
 */
long find_sys_file_table(void)
{
	int i = 0;
	for(; i<SYS_MAX_FILES && file_table[i].count; i++)
	{
		/* nothing, it's right */
	}
	return (i == SYS_MAX_FILES) ? -1 : i;
}

/*
 * �����û��ļ����������������ʧ�ܣ�����-1�����򷵻��û��ļ���������Ҳ������ֵ
 */
long find_user_file_table(void)
{
	int j = 0;
	for(; j<PROC_MAX_FILES && current->file[j]; j++)
	{
		/* nothing, it's right */
	}
	return (j == PROC_MAX_FILES) ? -1 : j;
}

/*
 * ����ϵͳ�ļ�������û��ļ�������������������á������û��ļ���������������ʧ�ܣ�����-1��
 */
long find_file_table(struct m_inode * p, unsigned short mode)
{
	int i = find_sys_file_table();
	if(-1 == i)
	{
		return -1;
	}
	/*
	 * ���ǽ����ҽ����ļ���������Ĳ�������������Է���һ...
	 */
	int j = find_user_file_table();
	if(-1 == j)
	{
		return -1;
	}
	/* �����ļ����� */
	file_table[i].inode = p;
	file_table[i].mode = mode;
	file_table[i].count = 1;
	file_table[i].offset = 0;
	/* ���ý����ļ��������� */
	current->file[j] = &file_table[i];

	return j;
}

/*
 * Open file which have been created.
 * I: �ļ�����������
 * O: �ļ����������������-1
 */
long open(char * pathname, unsigned short mode)
{
	char * name = pathname;
	struct m_inode * p = namei(name);
	if(!p)
	{
		printf("Kernel MSG: open: have no the file which named '%s'.\n", name);
		return -1;
	}
	if(FALSE == check_open_file_permit(p, mode))
	{
		printf("Kernel MSG: open: have no permit to open the file which named '%s'.\n", name);
		goto open_false_ret;
	}
	int j = find_file_table(p, mode);
	if(-1 == j)
	{
		printf("Kernel MSG: open: system-file-table or process-file-desc-table is full.\n");
		goto open_false_ret;
	}
	/* ������Ϊ���ļ������ͷ����д��̿� */
	if(O_CW == mode)
	{
		free_all(p);
		p->size = 0;
		p->state |= NODE_I_ALT;
		p->state |= NODE_D_ALT;
		/* �����ļ��޸�ʱ��� */
		/* nothing at present */
	}
	/* ������Ϊ׷��д���޸�ƫ���� */
	if(O_WA == mode)
	{
		current->file[j]->offset = p->size;
	}
	/*
	 * ���������ڵ㣬��namei�м�����NOTE! �˴�ȷʵֻ�ǽ�����������iput,��Ϊopen֮��
	 * �ļ���Ҫ��ʹ�ã����豣�ָ������ڵ����ڴ棬�����ܼ�С�����ü���ֵ��ʵ������
	 * close��iput�����Ǵ��������ڴ������ڵ�����ü���ֵ����������!
	 */
	UNLOCK_INODE(p);

	return j;

open_false_ret:

	iput(p);
	return -1;
}

/*
 * �������ļ�ʱ�����ļ��Ѵ��ڣ�����ʱ�����ԭ�����ļ����ݣ���˼�鴴���ļ���Ȩ���Ƿ�Ϸ���
 * NOTE! �ں˼�����ַǷ���������ļ�ΪĿ¼���ͣ��ǲ�����������յ�!!!
 */
BOOL check_creat_file_permit(struct m_inode * p, unsigned short mode)
{
	return ((p->mode & FILE_RONLY) || (p->mode & FILE_DIR)) ? FALSE : TRUE;
}

/*
 * �������ڵ�ź�·����������Ŀ¼�ļ��С��ú������Ȳ���Ŀ¼�ļ��Ŀ��
 * ���ҵ�������д����������Ŀ¼�ļ�ĩβд�롣�����ж�Ŀ¼��Ϊ��
 * �����������ļ����ֶ�Ϊ�գ������������������ڵ���Ƿ�Ϊ0�жϣ���Ϊ
 * �������ڵ�������ڵ�ž�Ϊ0.
 */
BOOL put_to_dir(struct m_inode * dir, unsigned long nrinode, char * path_part)
{
	unsigned long offset = 0;
	struct bmap_struct s_bmap = {0};
	struct buffer_head * bh = NULL;

//	d_printf("put_to_dir begin.\n");

	/* ��ȡĿ¼�ļ������Ŀ¼���жϣ����ҵ������д�� */
	/* �˲�����namei()�е����� */
	while(offset < dir->size)
	{
		if(bmap(dir, offset, &s_bmap))
		{
			if(s_bmap.nrblock)	/* ����� */
			{
				bh = bread(0, s_bmap.nrblock);
				/*
				 * NOTE! �����ں�Ĭ����һ��洢������Ŀ¼�д��ʱӦ���մ˹涨��
				 */
				while(s_bmap.offset + sizeof(struct file_dir_struct) <= s_bmap.bytes)
				{
					// Ŀ¼����ļ����ֶ�Ϊ��
					if(0 == strlen(((struct file_dir_struct *)&bh->p_data[s_bmap.offset])->name))
					{
						goto put_to_dir_out;
					}
					s_bmap.offset += sizeof(struct file_dir_struct);
				}
				brelse(bh);
			}
		}
		offset = (offset / 512 + 1) * 512;
	}
	/* ���û�ҵ����� */
	offset = (512 - (dir->size % 512)) < sizeof(struct file_dir_struct) ?
		((dir->size / 512 + 1) * 512) : dir->size;
	if(!check_alloc_data_block(dir, offset))	// ���������ݿ�ʧ��
	{
		return FALSE;
	}
	dir->size = offset + sizeof(struct file_dir_struct);
	bmap(dir, offset, &s_bmap);
	/* ��������ڵ����޸� */
	dir->state |= NODE_I_ALT;
	dir->state |= NODE_D_ALT;
	bh = bread(0, s_bmap.nrblock);

put_to_dir_out:		/* �ҵ������ת���� */

	((struct file_dir_struct *)&bh->p_data[s_bmap.offset])->nrinode = nrinode;
	strcpy(((struct file_dir_struct *)&bh->p_data[s_bmap.offset])->name, path_part);

//	d_printf("put_to_dir: bh->nrblock=%d, path='%s'.\n", bh->nrblock,
//		((struct file_dir_struct *)&bh->p_data[s_bmap.offset])->name);

	/* Ҫ���û����д����� */
	bh->state |= BUFF_DELAY_W;
	brelse(bh);				// gotoҲû���ͷ�

	return TRUE;
}

/*
 * ���ļ�������ʱ��creat�����øú���������һ���ļ���
 * NOTE! Ŀ¼�ǿ��Ա������ġ�
 */
struct m_inode * _creat(char * name, unsigned short mode)
{
	char * pstr = name;
	struct m_inode * dir = NULL;			// ��ǰĿ¼���ļ�
	struct m_inode * last_dir = NULL;		// ��һ��Ŀ¼
	char path[MAX_PATH+1] = {0};			// ��ǰȫ·��
	char path_part[MAX_PATH_PART+1] = {0};	// ��ǰ·������
	int i = 0, j = 0;
	unsigned short type = 0;				// �ļ�����

//	d_printf("_creat begin.\n");

	if('/' == *pstr)/* ��Ŀ¼ */
	{
		last_dir = iget(current->root_dir->dev, current->root_dir->nrinode);
		UNLOCK_INODE(last_dir);
		path[i++] = *(pstr++);
	}
	else/* ��ǰĿ¼ */
	{
		last_dir = iget(current->current_dir->dev, current->current_dir->nrinode);
		UNLOCK_INODE(last_dir);
	}
	while(*pstr)
	{
		j = 0;
		type = 0;
		/* ��ȡһ��·������ */
		while(*pstr && *pstr!='/')
		{
			path_part[j++] = path[i++] = *(pstr++);
		}
		if(*pstr)
		{
			type |= FILE_DIR;
			path[i++] = *(pstr++);
		}
		path[i] = 0;
		path_part[j] = 0;
		/* ��ǰĿ¼���ļ����� */
		if(dir = namei(path))
		{
			iput(last_dir);
			last_dir = dir;
			/*
			 * ���������ڵ㣬�Ա�namei�����һ�β���������ΪҪ���������������ڵ㡣
			 */
			*pstr ? ({UNLOCK_INODE(dir);}) : 0;
			continue;
		}
		/* ��ǰĿ¼���ļ������� */
		if(!(dir = ialloc(0)))
		{
			printf("Kernel MSG: _creat: ialloc return NULL.\n");
			goto _creat_false_ret;
		}
		/* ����Ŀ¼���ͣ����������1,2��Ŀ¼��Ϊ '.', '..' */
		if(type & FILE_DIR)
		{
			if(!put_to_dir(dir, dir->nrinode, ".") || !put_to_dir(dir, last_dir->nrinode, ".."))
			{
				printf("Kernel MSG: _creat: put_to_dir return FALSE.\n");
				goto _creat_false_ret;
			}
			dir->mode |= FILE_DIR;
		}
		else
		{
			dir->mode |= FILE_FILE;
		}
		dir->mode |= mode;
		if(!put_to_dir(last_dir, dir->nrinode, path_part))
		{
			printf("Kernel MSG: _creat: put_to_dir return FALSE.\n");
			goto _creat_false_ret;
		}
		last_dir->state |= NODE_I_ALT;
		last_dir->state |= NODE_D_ALT;
		iput(last_dir);
		last_dir = dir;
		*pstr ? ({UNLOCK_INODE(dir);}) : 0;
	}

	return dir;

_creat_false_ret:

	last_dir ? iput(last_dir) : 0;
	dir ? iput(dir) : 0;
	return (struct m_inode *)NULL;
}

/*
 * Create file.
 * I: �ļ�����������
 * O: �ļ����������������-1
 */
long creat(char * pathname, unsigned short mode)
{
	char * name = pathname;
	if(!name)
	{
		printf("Kernel MSG: creat: path-name is NULL.\n");
		return -1;
	}
	
	struct m_inode * p = namei(name);

//	d_printf("creat: namei return.\n");

	if(p)/* �ļ��Ѵ��� */
	{
		if(!(check_creat_file_permit(p, mode)))
		{
			printf("Kernel MSG: creat: have no permit to visit the file which named '%s'.\n", name);
			goto creat_false_ret;
		}
		free_all(p);
		p->size = 0;
	}
	else/* �ļ������� */
	{
		if(!(p = _creat(name, mode)))
		{
			printf("Kernel MSG: creat: call _creat and return NULL.\n");
			return -1;
		}
	}
	int j = find_file_table(p, mode);
	if(-1 == j)
	{
		printf("Kernel MSG: open: system-file-table or process-file-desc-table is full.\n");
		goto creat_false_ret;
	}

	p->mode |= mode;
	p->state |= NODE_I_ALT;
	p->state |= NODE_D_ALT;
	/* �����ļ��޸�ʱ��� */
	/* nothing at present */
	UNLOCK_INODE(p);

	return j;

creat_false_ret:

	iput(p);
	return -1;
}

/*
 * Well, we begin to read file. ����֪�����ļ����пն���������������ƫ���ļ�ָ�룬�Ա��д��
 * ͬ�����ڶ�ȡ�ļ�ʱ���������ն��������û����������0.
 * I: �û��ļ����������û������еĻ�������ַ��Ҫ�����ֽ���
 * O: �������û������ֽ���
 */
long read(long desc, char * buff, unsigned long count)
{
	struct file_table_struct * p_file = current->file[desc];
	if(!p_file)
	{
		printf("Kernel MSG: read: trying to read unknow-file.\n");
		return -1;
	}
	/* ������Ȩ�� */
	/* nothing at present */
	struct m_inode * p = p_file->inode;
	if(!p)
	{
		panic("read: BAD BAD, process-file-desc is OK, but the pointer of memory inode is NULL!");
	}
	LOCK_INODE(p);
	struct buffer_head * bh = NULL;
	unsigned long readcount = 0;	// �Ѷ�ȡ���ֽ���
	unsigned long oneread = 0;		// һ��Ҫ��ȡ���ֽ���
	struct bmap_struct s_bmap = {0};
	/* ��ʼ��ȡ */
	while(readcount < count && p_file->offset < p->size)
	{
		if(!bmap(p, p_file->offset, &s_bmap))
		{
			panic("read: bmap return FALSE.");
		}
		/* ����һ�ζ�ȡ���ֽ��� */
		oneread = 512;
		oneread = (oneread < (count - readcount)) ? oneread : (count - readcount);
		oneread = (oneread < (p->size - p_file->offset)) ? oneread : (p->size - p_file->offset);
		oneread = (oneread < (512 - s_bmap.offset)) ? oneread : (512 - s_bmap.offset);
		/* ��ȡһ�� */
		if(NULL == s_bmap.nrblock)/* �����ļ��ն� */
		{
			for(int i=0; i<oneread; i++)
			{
				buff[readcount++] = 0;
			}
		}
		else/* �ǿն� */
		{
//			d_printf("read: s_bmap.nrblock=%d.\n", s_bmap.nrblock);

			bh = bread(0, s_bmap.nrblock);
			memcpy(&buff[readcount], &bh->p_data[s_bmap.offset], oneread);
			brelse(bh);
			readcount += oneread;
		}
		p_file->offset += oneread;
	}
	if(readcount)
	{
		/* �޸ķ���ʱ��� */
		/* nothing at present */
	}
	UNLOCK_INODE(p);

//	d_printf("read: read-count = %d.\n", readcount);

	return readcount;
}

/*
 * Write file. �����Լ�����ֵͬread. Well, write file and read file is simple. 
 * ���ǣ�������һЩ�Ż�������д�������ݣ��򲻱ض�ȡ�ÿ飬ֻ��д�뼴�ɣ�����ֻ
 * д��ÿ��һ���֣��������ÿ顣
 * NOTE! Ŀ¼�ǲ���д��!
 */
long write(long desc, char * buff, unsigned long count)
{
	if(!buff && count>0)
	{
		printf("Kernel MSG: write: write-count > 0. but, write-buffer-pointer is NULL.\n");
		return -1;
	}
	struct file_table_struct * p_file = current->file[desc];
	if(!p_file)
	{
		printf("Kernel MSG: write: trying to write unknow-file.\n");
		return -1;
	}
	struct m_inode * p = p_file->inode;
	/* ������Ȩ�� */
	if(O_RONLY == p_file->mode || (p->mode & FILE_DIR))
	{
		printf("Kernel MSG: write: have no permit to write the file.\n");
		return -1;
	}
	if(!p)
	{
		panic("write: BAD BAD, process-file-desc is OK, but the pointer of memory inode is NULL!");
	}
	LOCK_INODE(p);
	struct buffer_head * bh = NULL;
	unsigned long writecount = 0;	// ��д���ֽ���
	unsigned long onewrite = 0;		// һ��Ҫд���ֽ���
	struct bmap_struct s_bmap = {0};
	/* ��ʼд */
	while(writecount < count)
	{
		/* ������ݿ���ӿ��Ƿ���ڣ�������������� */
		if(!check_alloc_data_block(p, p_file->offset))
		{
			printf("Kernel MSG: write: check-alloc-disk-block ERROR!\n");
			return -1;
		}

		bmap(p, p_file->offset, &s_bmap);
		/* ����һ��д���ֽ��� */
		onewrite = 512;
		onewrite = (onewrite < (count - writecount)) ? onewrite : (count - writecount);
		onewrite = (onewrite < (512 - s_bmap.offset)) ? onewrite : (512 - s_bmap.offset);

		/* дһ�� */
		if(512 == onewrite)
		{
			bh = getblk(0, s_bmap.nrblock);
		}
		else
		{
//			d_printf("write: s_bmap.nrblock=%d.\n", s_bmap.nrblock);

			bh = bread(0, s_bmap.nrblock);
		}
		memcpy(&bh->p_data[s_bmap.offset], &buff[writecount], onewrite);
		bh->state |= BUFF_DELAY_W;	// �ӳ�д
		brelse(bh);

		writecount += onewrite;
		p_file->offset += onewrite;
	}
	/* �ļ�������޸�size�ֶ� */
	p->size = (p->size > p_file->offset) ? p->size : p_file->offset;
	if(writecount)
	{
		p->state |= NODE_I_ALT;
		p->state |= NODE_D_ALT;
		/* �޸��ļ�ʱ��� */
		/* nothing at present */
	}
	UNLOCK_INODE(p);

//	d_printf("write: file-size = %d.\n", p->size);

	return writecount;
}

/*
 * �ļ�����/���λ�õĵ���������referenceΪ�ֽ�ƫ��������λ�á�
 * lseek�жϵ�ǰ�򿪵��ļ�״̬������ֻ������ƫ���������Գ���size��
 * �������...
 */
long lseek(long desc, long offset, unsigned char reference)
{
	struct file_table_struct * p_file = current->file[desc];
	if(!p_file)
	{
		printf("Kernel MSG: lseek: trying to handle unknow-file.\n");
		return -1;
	}
	struct m_inode * p = p_file->inode;
	if(!p)
	{
		panic("lseek: BAD BAD, process-file-desc is OK, but the pointer of memory inode is NULL!");
	}
	if(reference!=SEEK_SET && reference!=SEEK_CUR && reference!=SEEK_END)
	{
		printf("Kernel MSG: lseek: BAD reference.\n");
		return -1;
	}

	offset += reference==SEEK_SET ? 0 : (reference==SEEK_CUR ? p_file->offset : p->size);
	offset = offset<0 ? 0 : offset;
	offset = (O_RONLY==p_file->mode && offset>p->size) ? p->size : offset;
	p_file->offset = offset;

//	d_printf("lseek: p_file->offset = %d.\n", p_file->offset);

	return p_file->offset;
}

long close(long desc)
{
	struct file_table_struct * p_file = current->file[desc];
	if(!p_file)
	{
		printf("Kernel MSG: close: trying to close unknow-file.\n");
		return -1;
	}
	struct m_inode * p = p_file->inode;
	if(!p)
	{
		panic("close: BAD BAD, process-file-desc is OK, but the pointer of memory inode is NULL!");
	}

	current->file[desc] = NULL;
	if(--p_file->count > 0)
	{
		return 1;
	}
	/*
	 * ��ϵͳ�ļ���������ü���Ϊ0ʱ���ں˽����ͷţ����ں˲���տ��е�ϵͳ�ļ����
	 * �ں��ٴ�Ѱ�ҿ��б���ʱ�����������ü���ֵ��ȷ���Ƿ���С�
	 */
	iput(p);

	return 1;
}


void file_table_init(void)
{
	zeromem(file_table, sizeof(struct file_table_struct) * SYS_MAX_FILES);
}
