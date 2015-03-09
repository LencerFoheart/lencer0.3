/*
 * Small/fs/read_write.c
 *
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-3-3 12:04:48
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * NOTE! 文件系统的系统调用一般情况下，出错返回-1.
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

struct file_table_struct file_table[SYS_MAX_FILES] = {{0,},};	// 系统文件表

/*
 * 检查文件打开类型是否合法。此处只过滤非法的，其余皆为合法。
 * NOTE! 内核检查这种非法情况，当文件为目录类型，是不允许被
 * 进程以可写的方式打开的!!!
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
 * 分配系统文件表项。若分配失败，返回-1，否则返回索引值
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
 * 分配用户文件描述符表项。若分配失败，返回-1，否则返回用户文件描述符，也即索引值
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
 * 分配系统文件表项和用户文件描述符表项，并进行设置。返回用户文件描述符，若分配失败，返回-1。
 */
long find_file_table(struct m_inode * p, unsigned short mode)
{
	int i = find_sys_file_table();
	if(-1 == i)
	{
		return -1;
	}
	/*
	 * 我们将查找进程文件描述符表的操作，紧随其后，以防万一...
	 */
	int j = find_user_file_table();
	if(-1 == j)
	{
		return -1;
	}
	/* 设置文件表项 */
	file_table[i].inode = p;
	file_table[i].mode = mode;
	file_table[i].count = 1;
	file_table[i].offset = 0;
	/* 设置进程文件描述符表 */
	current->file[j] = &file_table[i];

	return j;
}

/*
 * Open file which have been created.
 * I: 文件名，打开类型
 * O: 文件描述符，错误输出-1
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
	/* 打开类型为清文件，则释放所有磁盘块 */
	if(O_CW == mode)
	{
		free_all(p);
		p->size = 0;
		p->state |= NODE_I_ALT;
		p->state |= NODE_D_ALT;
		/* 设置文件修改时间等 */
		/* nothing at present */
	}
	/* 打开类型为追加写，修改偏移量 */
	if(O_WA == mode)
	{
		current->file[j]->offset = p->size;
	}
	/*
	 * 解锁索引节点，在namei中加锁。NOTE! 此处确实只是解锁，并不是iput,因为open之后，
	 * 文件还要被使用，故需保持该索引节点在内存，即不能减小其引用计数值。实际上在
	 * close中iput，正是此做法让内存索引节点的引用计数值发挥了作用!
	 */
	UNLOCK_INODE(p);

	return j;

open_false_ret:

	iput(p);
	return -1;
}

/*
 * 当创建文件时，若文件已存在，创建时将清除原来的文件内容，因此检查创建文件的权限是否合法。
 * NOTE! 内核检查这种非法情况，当文件为目录类型，是不允许被进程清空的!!!
 */
BOOL check_creat_file_permit(struct m_inode * p, unsigned short mode)
{
	return ((p->mode & FILE_RONLY) || (p->mode & FILE_DIR)) ? FALSE : TRUE;
}

/*
 * 将索引节点号和路径分量放入目录文件中。该函数首先查找目录文件的空项，
 * 若找到，则将其写入空项；否则，在目录文件末尾写入。我们判断目录项为空
 * 的依据是其文件名字段为空，而不能依据其索引节点号是否为0判断，因为
 * 根索引节点的索引节点号就为0.
 */
BOOL put_to_dir(struct m_inode * dir, unsigned long nrinode, char * path_part)
{
	unsigned long offset = 0;
	struct bmap_struct s_bmap = {0};
	struct buffer_head * bh = NULL;

//	d_printf("put_to_dir begin.\n");

	/* 读取目录文件，逐个目录项判断，若找到空项，就写入 */
	/* 此部分与namei()中的相似 */
	while(offset < dir->size)
	{
		if(bmap(dir, offset, &s_bmap))
		{
			if(s_bmap.nrblock)	/* 块存在 */
			{
				bh = bread(0, s_bmap.nrblock);
				/*
				 * NOTE! 本版内核默认是一块存储整数个目录项，写入时应按照此规定。
				 */
				while(s_bmap.offset + sizeof(struct file_dir_struct) <= s_bmap.bytes)
				{
					// 目录项的文件名字段为空
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
	/* 如果没找到空项 */
	offset = (512 - (dir->size % 512)) < sizeof(struct file_dir_struct) ?
		((dir->size / 512 + 1) * 512) : dir->size;
	if(!check_alloc_data_block(dir, offset))	// 若分配数据块失败
	{
		return FALSE;
	}
	dir->size = offset + sizeof(struct file_dir_struct);
	bmap(dir, offset, &s_bmap);
	/* 标记索引节点已修改 */
	dir->state |= NODE_I_ALT;
	dir->state |= NODE_D_ALT;
	bh = bread(0, s_bmap.nrblock);

put_to_dir_out:		/* 找到空项，跳转至此 */

	((struct file_dir_struct *)&bh->p_data[s_bmap.offset])->nrinode = nrinode;
	strcpy(((struct file_dir_struct *)&bh->p_data[s_bmap.offset])->name, path_part);

//	d_printf("put_to_dir: bh->nrblock=%d, path='%s'.\n", bh->nrblock,
//		((struct file_dir_struct *)&bh->p_data[s_bmap.offset])->name);

	/* 要将该缓冲块写入磁盘 */
	bh->state |= BUFF_DELAY_W;
	brelse(bh);				// goto也没有释放

	return TRUE;
}

/*
 * 当文件不存在时，creat将调用该函数，创建一个文件。
 * NOTE! 目录是可以被创建的。
 */
struct m_inode * _creat(char * name, unsigned short mode)
{
	char * pstr = name;
	struct m_inode * dir = NULL;			// 当前目录或文件
	struct m_inode * last_dir = NULL;		// 上一级目录
	char path[MAX_PATH+1] = {0};			// 当前全路径
	char path_part[MAX_PATH_PART+1] = {0};	// 当前路径分量
	int i = 0, j = 0;
	unsigned short type = 0;				// 文件类型

//	d_printf("_creat begin.\n");

	if('/' == *pstr)/* 根目录 */
	{
		last_dir = iget(current->root_dir->dev, current->root_dir->nrinode);
		UNLOCK_INODE(last_dir);
		path[i++] = *(pstr++);
	}
	else/* 当前目录 */
	{
		last_dir = iget(current->current_dir->dev, current->current_dir->nrinode);
		UNLOCK_INODE(last_dir);
	}
	while(*pstr)
	{
		j = 0;
		type = 0;
		/* 获取一个路径分量 */
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
		/* 当前目录或文件存在 */
		if(dir = namei(path))
		{
			iput(last_dir);
			last_dir = dir;
			/*
			 * 解锁索引节点，以便namei。最后一次不解锁，因为要返回上锁的索引节点。
			 */
			*pstr ? ({UNLOCK_INODE(dir);}) : 0;
			continue;
		}
		/* 当前目录或文件不存在 */
		if(!(dir = ialloc(0)))
		{
			printf("Kernel MSG: _creat: ialloc return NULL.\n");
			goto _creat_false_ret;
		}
		/* 若是目录类型，则设置其第1,2个目录项为 '.', '..' */
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
 * I: 文件名，打开类型
 * O: 文件描述符，错误输出-1
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

	if(p)/* 文件已存在 */
	{
		if(!(check_creat_file_permit(p, mode)))
		{
			printf("Kernel MSG: creat: have no permit to visit the file which named '%s'.\n", name);
			goto creat_false_ret;
		}
		free_all(p);
		p->size = 0;
	}
	else/* 文件不存在 */
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
	/* 设置文件修改时间等 */
	/* nothing at present */
	UNLOCK_INODE(p);

	return j;

creat_false_ret:

	iput(p);
	return -1;
}

/*
 * Well, we begin to read file. 我们知道，文件会有空洞，这样允许随意偏移文件指针，以便读写。
 * 同样，在读取文件时，若遇到空洞，则向用户缓冲区中填补0.
 * I: 用户文件描述符，用户进程中的缓冲区地址，要读的字节数
 * O: 拷贝到用户区的字节数
 */
long read(long desc, char * buff, unsigned long count)
{
	struct file_table_struct * p_file = current->file[desc];
	if(!p_file)
	{
		printf("Kernel MSG: read: trying to read unknow-file.\n");
		return -1;
	}
	/* 检查许可权限 */
	/* nothing at present */
	struct m_inode * p = p_file->inode;
	if(!p)
	{
		panic("read: BAD BAD, process-file-desc is OK, but the pointer of memory inode is NULL!");
	}
	LOCK_INODE(p);
	struct buffer_head * bh = NULL;
	unsigned long readcount = 0;	// 已读取的字节数
	unsigned long oneread = 0;		// 一次要读取的字节数
	struct bmap_struct s_bmap = {0};
	/* 开始读取 */
	while(readcount < count && p_file->offset < p->size)
	{
		if(!bmap(p, p_file->offset, &s_bmap))
		{
			panic("read: bmap return FALSE.");
		}
		/* 计算一次读取的字节数 */
		oneread = 512;
		oneread = (oneread < (count - readcount)) ? oneread : (count - readcount);
		oneread = (oneread < (p->size - p_file->offset)) ? oneread : (p->size - p_file->offset);
		oneread = (oneread < (512 - s_bmap.offset)) ? oneread : (512 - s_bmap.offset);
		/* 读取一次 */
		if(NULL == s_bmap.nrblock)/* 处理文件空洞 */
		{
			for(int i=0; i<oneread; i++)
			{
				buff[readcount++] = 0;
			}
		}
		else/* 非空洞 */
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
		/* 修改访问时间等 */
		/* nothing at present */
	}
	UNLOCK_INODE(p);

//	d_printf("read: read-count = %d.\n", readcount);

	return readcount;
}

/*
 * Write file. 参数以及返回值同read. Well, write file and read file is simple. 
 * 但是，我们做一些优化，若是写整块数据，则不必读取该块，只需写入即可，若是只
 * 写入该块的一部分，则需读入该块。
 * NOTE! 目录是不可写的!
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
	/* 检查许可权限 */
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
	unsigned long writecount = 0;	// 已写的字节数
	unsigned long onewrite = 0;		// 一次要写的字节数
	struct bmap_struct s_bmap = {0};
	/* 开始写 */
	while(writecount < count)
	{
		/* 检查数据块或间接块是否存在，若不存在则分配 */
		if(!check_alloc_data_block(p, p_file->offset))
		{
			printf("Kernel MSG: write: check-alloc-disk-block ERROR!\n");
			return -1;
		}

		bmap(p, p_file->offset, &s_bmap);
		/* 计算一次写的字节数 */
		onewrite = 512;
		onewrite = (onewrite < (count - writecount)) ? onewrite : (count - writecount);
		onewrite = (onewrite < (512 - s_bmap.offset)) ? onewrite : (512 - s_bmap.offset);

		/* 写一次 */
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
		bh->state |= BUFF_DELAY_W;	// 延迟写
		brelse(bh);

		writecount += onewrite;
		p_file->offset += onewrite;
	}
	/* 文件变大，则修改size字段 */
	p->size = (p->size > p_file->offset) ? p->size : p_file->offset;
	if(writecount)
	{
		p->state |= NODE_I_ALT;
		p->state |= NODE_D_ALT;
		/* 修改文件时间等 */
		/* nothing at present */
	}
	UNLOCK_INODE(p);

//	d_printf("write: file-size = %d.\n", p->size);

	return writecount;
}

/*
 * 文件输入/输出位置的调整。其中reference为字节偏移量依据位置。
 * lseek判断当前打开的文件状态，若是只读，则偏移量不可以超出size，
 * 否则可以...
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
	 * 当系统文件表项的引用计数为0时，内核将它释放，但内核不清空空闲的系统文件表项，
	 * 内核再次寻找空闲表项时，依据其引用计数值来确定是否空闲。
	 */
	iput(p);

	return 1;
}


void file_table_init(void)
{
	zeromem(file_table, sizeof(struct file_table_struct) * SYS_MAX_FILES);
}
