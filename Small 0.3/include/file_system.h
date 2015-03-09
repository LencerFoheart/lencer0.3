/*
 * Small/include/file_system.h
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-02-18 19:27:08
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

#ifndef _FILE_SYSTEM_H_
#define _FILE_SYSTEM_H_

#include "types.h"

//**********************************************************************

#define BUFFER_SIZE		512

/* buffer state */
#define BUFF_LOCK		(1 << 0)	// ����
#define BUFF_VAL		(1 << 1)	// ������Ч����
#define BUFF_DELAY_W	(1 << 2)	// �ӳ�д

/* inode state */
#define NODE_LOCK		(1 << 0)	// ����
#define NODE_I_ALT		(1 << 1)	// �����ڵ����޸�
#define NODE_D_ALT		(1 << 2)	// �ļ��������޸�
#define NODE_MOUNT		(1 << 3)	// ���ļ��Ƿ�װ
#define NODE_VAL		(1 << 4)	// ��Ч

/* super-block state */
#define SUP_LOCK		(1 << 0)	// ��������
#define SUP_ALT			(1 << 1)	// ���������޸�

/* [inode] file type & file permit */
#define FILE_DIR		(1 << 0)	// Ŀ¼
#define FILE_FILE		(1 << 1)	// �ļ�
#define FILE_RONLY		(1 << 2)	// ֻ��
#define FILE_RW			(1 << 3)	// ��д
#define FILE_EXEC		(1 << 4)	// ��ִ��

/* [open] file open type */
#define O_RONLY		1				// ֻ��
#define O_WONLY		2				// ֻд���൱���޸ģ�����׷�ӣ�Ҳ����ȫ��
#define O_RW		3				// ��д��дʱ�൱���޸ģ�����׷�ӣ�Ҳ����ȫ��
#define O_CW		4				// ���֮��д
#define O_WA		5				// ��׷��д�ķ�ʽ��

/* lseek type */
#define SEEK_SET		0
#define SEEK_CUR		1
#define SEEK_END		2

/*
 * NOTE! ����·��������󳤶Ⱥ�����·�����ȵ����ã���ʹһ�����̿�(512bytes)�ܴ洢������Ŀ¼��Ϊ��!
 */
#define MAX_PATH			280		// ���·��������
#define MAX_PATH_PART		28		// ���·����������
#define PROC_MAX_FILES		50		// һ�����̴��ļ������
#define SYS_MAX_FILES		256		// ϵͳ���ļ������

#define NR_BUFF_HASH		200			// �ڴ��л�������ϣ����������
#define NR_INODE_HASH		15			// �ڴ���inode��ϣ����������
#define NR_INODE			50			// �ڴ���inode��������

#define NR_D_INODE			300			// �����������ڵ���������洢���ļ�����
#define NR_D_FBT			90			// �����п��п������
#define NR_D_FIT			10			// �����п��������ڵ������

#define END_D_FB			0			// �����п��п������־


//**********************************************************************

// ���������ڵ��Ӧ���̿顣2Ϊ�����ڵ㿪ʼ�߼����̿飬�����ڵ�Ŵ�0��ʼ��
#define INODE_TO_BLOCK(nrinode) (2 + (nrinode / (512 / sizeof(struct d_inode))))

// ���������ڵ��ڴ��̿��е�ƫ�����������ڵ�Ŵ�0��ʼ��
#define INODE_START_IN_BLOCK(nrinode) (sizeof(struct d_inode) * (nrinode % (512 / sizeof(struct d_inode))))

// ���¶���ṹ�����ڼ�¼bmap�����Ĳ�������¼�ļ��߼��ֽ�ƫ�������ļ����̿��ӳ��
struct bmap_struct {
	unsigned long nrblock;
	unsigned long b_index;			// ���յ����ݿ���ǰһ����ӿ������
	unsigned long offset;			// �����ֽ�ƫ����
	unsigned long bytes;			// ���п�ʹ���ֽ������ӿ鿪ʼ������������ļ��������ֽ���
	unsigned char step;				// ��Ӽ�
	unsigned long end_jmp_nrblock;	// ���μ�ַ�����μ�ַ�����һ����ַ��Ŀ��
	unsigned long end_b_index;		// ���μ�ַ�����μ�ַ�����һ����ַ��Ŀ�� ����һ����ַ�е�����ֵ
	unsigned long mid_jmp_nrblock;	// ���μ�ַ�м�һ����ַ��Ŀ��
	unsigned long mid_b_index;		// ���μ�ַ�м�һ����ַ��Ŀ�� ����һ����ַ�е�����ֵ
};

/*
 * Ŀ¼�ļ���Ŀ¼����ṹ�������ں�Ĭ����һ��洢������Ŀ¼�
 * �����ж�Ŀ¼��Ϊ�յ����������ļ����ֶ�Ϊ�գ�����������������
 * �ڵ���Ƿ�Ϊ0�жϣ���Ϊ�������ڵ�������ڵ�ž�Ϊ0�����ԣ���
 * д��Ŀ¼������Ŀ¼��ʱ����Ҫ�鿴��������ļ����ֶΡ�
 */
struct file_dir_struct {
	unsigned long nrinode;
	char name[MAX_PATH_PART];	// Ϊ�գ����ʾ�������
};

// ϵͳ�ļ������ṹ
struct file_table_struct {
	struct m_inode * inode;	// �����ڵ�ָ��
	unsigned short mode;	// ��дȨ�޵�
	unsigned char count;	// ���������Ǹñ����Ƿ���еı�־
	unsigned long offset;	// ƫ����
};

//**********************************************************************

// ����ͷ��
struct buffer_head {
	unsigned char dev;						// �豸��
	unsigned long nrblock;					// ���
	unsigned char state;					// ״̬
	unsigned char * p_data;					// ָ����������
	struct buffer_head *p_hash_next;		// ɢ�ж����ϵ���һ��������ָ��
	struct buffer_head *p_hash_pre;			// ɢ�ж����ϵ���һ��������ָ��
	struct buffer_head *p_next_free;		// ���б��ϵ���һ��������ָ��
	struct buffer_head *p_pre_free;			// ���б��ϵ���һ��������ָ��
};

//**********************************************************************

// ���������ڵ㣬ÿ�����̿�ֻ�洢�����������ڵ�
struct d_inode {
	unsigned short mode;			// �ļ����͡���ȡȨ��
	unsigned long create_t;			// ����ʱ��
	unsigned long use_t;			// ���һ��ʹ��ʱ��
	unsigned long alter_t;			// ���һ���޸�ʱ��
	unsigned long i_alter_t;		// �����ڵ����һ���޸�ʱ��
	unsigned char nlinks;			// ������
	unsigned long size;				// �ļ���С���ֽ�
	unsigned long zone[13];			// 0-9ֱ�ӣ�10һ�μ�ַ��11���Σ�12����
};

// �ڴ������ڵ�
struct m_inode {
	unsigned short mode;			// �ļ����͡���ȡȨ��
	unsigned long create_t;			// ����ʱ��
	unsigned long use_t;			// ���һ��ʹ��ʱ��
	unsigned long alter_t;			// ���һ���޸�ʱ��
	unsigned long i_alter_t;		// �����ڵ����һ���޸�ʱ��
	unsigned char nlinks;			// ������
	unsigned long size;				// �ļ���С���ֽ�
	unsigned long zone[13];			// 0-9ֱ�ӣ�10һ�μ�ַ��11���Σ�12����
	/* the above same to disk */
	unsigned char count;				// ������
	unsigned char state;				// �ڴ������ڵ�״̬
	unsigned char dev;					// �豸��
	unsigned long nrinode;				// �����ڵ��
	struct m_inode *p_hash_next;		// ɢ�ж����ϵ���һ��������ָ��
	struct m_inode *p_hash_pre;			// ɢ�ж����ϵ���һ��������ָ��
	struct m_inode *p_next_free;		// ���б��ϵ���һ��������ָ��
	struct m_inode *p_pre_free;			// ���б��ϵ���һ��������ָ��
};

//**********************************************************************

// ���̳�����
struct d_super_block {
	unsigned long size;				// �ļ�ϵͳ�ߴ磬���̿���
	unsigned long nfreeblocks;		// �ļ�ϵͳ�п��п����
	unsigned long fb_table[NR_D_FBT];	// ���п�����п�������
	unsigned char nextfb;			// ���п������һ�����п�
	unsigned long ninodes;			// �����ڵ���С�������ڵ����
	unsigned long nfreeinodes;		// ���������ڵ����
	unsigned long fi_table[NR_D_FIT];	// ���������ڵ�����������ڵ�������
	unsigned char nextfi;			// ���������ڵ������һ�����������ڵ�
	unsigned long store_nexti;		// ���ǵ������ڵ������һ����ʼ�����������ڵ��
};

// �ڴ泬����
struct m_super_block {
	unsigned long size;				// �ļ�ϵͳ�ߴ磬���̿���
	unsigned long nfreeblocks;		// �ļ�ϵͳ�п��п����
	unsigned long fb_table[NR_D_FBT];	// ���п�����п�������
	unsigned char nextfb;			// ���п������һ�����п�
	unsigned long ninodes;			// �����ڵ���С�������ڵ����
	unsigned long nfreeinodes;		// ���������ڵ����
	unsigned long fi_table[NR_D_FIT];	// ���������ڵ�����������ڵ�������
	unsigned char nextfi;			// ���������ڵ������һ�����������ڵ�
	unsigned long store_nexti;		// ���ǵ������ڵ������һ����ʼ�����������ڵ��
	/* the above same to disk */
	unsigned char state;			// ״̬
};

//**********************************************************************

extern struct proc_struct * inode_wait;
extern struct proc_struct * buff_wait;

#define LOCK_BUFF(p) \
do \
{ \
	(p)->state |= BUFF_LOCK; \
}while(0)

#define UNLOCK_BUFF(p) \
do \
{ \
	(p)->state &= (~BUFF_LOCK) & 0xff; \
	if(NULL != buff_wait) \
	{ \
		wake_up(&buff_wait); \
	} \
}while(0)


#define LOCK_INODE(p) \
do \
{ \
	(p)->state |= NODE_LOCK; \
}while(0)

#define UNLOCK_INODE(p) \
do \
{ \
	(p)->state &= (~NODE_LOCK) & 0xff; \
	if(NULL != inode_wait) \
	{ \
		wake_up(&inode_wait); \
	} \
}while(0)


#define LOCK_SUPER(p) \
do \
{ \
	(p)->state |= SUP_LOCK; \
}while(0)

#define UNLOCK_SUPER(p) \
do \
{ \
	(p)->state &= (~SUP_LOCK) & 0xff; \
	if(NULL != super_wait) \
	{ \
		wake_up(&super_wait); \
	} \
}while(0)

/* buffer */
extern struct buffer_head * getblk(unsigned char dev, unsigned long nrblock);
extern void brelse(struct buffer_head *p);
extern struct buffer_head * bread(unsigned char dev, unsigned long nrblock);
extern void bwrite(struct buffer_head *p);
extern void buff_init(void);

/* inode */
extern void read_inode(struct m_inode * p, unsigned char dev, unsigned int nrinode);
extern void write_inode(struct m_inode * p, unsigned char dev, unsigned int nrinode, char isnow);
extern struct m_inode * iget(unsigned char dev, unsigned int nrinode);
extern void iput(struct m_inode * p);
extern BOOL bmap(struct m_inode * p, unsigned long v_offset, struct bmap_struct * s_bmap);
extern struct m_inode * namei(char * name);
extern void inode_init(void);

/* alloc */
extern struct m_inode * ialloc(unsigned char dev);
extern void ifree(unsigned char dev, unsigned int nrinode);
extern struct buffer_head * alloc(unsigned char dev);
extern BOOL check_alloc_data_block(struct m_inode * p, unsigned long offset);
extern void free(unsigned char dev, unsigned long nrblock);
extern void free_all(struct m_inode * p);
extern void super_init(void);


/* read_write */
extern long open(char * pathname, unsigned short mode);
extern BOOL put_to_dir(struct m_inode * dir, unsigned long nrinode, char * path_part);
extern struct m_inode * _creat(char * name, unsigned short mode);
extern long creat(char * pathname, unsigned short mode);
extern long read(long desc, char * buff, unsigned long count);
extern long write(long desc, char * buff, unsigned long count);
extern long lseek(long desc, long offset, unsigned char reference);
extern long close(long desc);
extern void file_table_init(void);

//**********************************************************************

#endif // _FILE_SYSTEM_H_