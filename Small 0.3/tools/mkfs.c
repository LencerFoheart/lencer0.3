/*
 * Small/tools/mkfs.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-02-18 23:58:43
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * ���ļ������ļ�ϵͳ���ɳ��򡣴˴������ļ���Ϊ���̾�����ģ����̡�
 * ���ļ�ϵͳ������UNIX system V���ļ�ϵͳ���ں�Ĭ�������ڵ�Ŵ�0��ʼ��
 */

#include <stdio.h>						// ��׼ͷ�ļ������Խ�
#include <string.h>						// ��׼ͷ�ļ������Խ�
#include "../include/file_system.h"		// �����ļ�ϵͳ�Ĵ�������

#define DISK_SIZE		10				// ���ڹ����ļ�ϵͳ�Ĵ��̵Ĵ�С��MB��NOTE! �˴���������ʵ�ĳߴ硣

/*
 * NOTE! �������� DISK_SIZE ��һ��ʹ���ɵ�Ӳ�̾�������ʵ��Ӳ����ƥ�䣬�ŵ����˴���
 * ����Ӳ�̣���ͷ����ÿ�ŵ����������ŵ�����Ӧ���������������¶��塣
 *
 * ��PC��׼����һ������Ȥ������Ϊ�˼��ݴ�����ϵ�BIOS����������
 * ������ͨ��ִ��һ���ڲ�ת�룬ת��Ϊ�߼���ÿ�ŵ�63����������ˣ�
 * �������ǽ��ŵ�����������Ϊ63���Ա����VMware��(ע: Bochs������
 * Ϊ63Ҳ�ǿ��Ե�)��
 */
#define HEADS			2				// ����Ĭ�ϵĴ�ͷ��
#define SPT				63				// ����Ĭ�ϵ�ÿ�ŵ������������޸���
#define CYLINDERS		(DISK_SIZE * 1024 * 1024 / 512 / HEADS / SPT)	// ���̴ŵ���

void mkfs(void)
{
	static char canrun = 1;				// Ϊ1ʱ���ú�������ִ��
	FILE * fp = NULL;
	unsigned long i=0;					// ѭ�����Ʊ���
	unsigned long pfreeblock = 0;		// ��������һ���账��Ŀ��п�Ŀ��
	char buff[512] = {0};
	struct d_super_block sup_b = {0};	// ������
	struct d_inode root = {0};			// �������ڵ�
	struct file_dir_struct dir = {0};	// �������ڵ��Ŀ¼����

	if(!canrun)
	{
		return;
	}
	canrun = 0;

	if(!(fp = fopen("rootImage","wb")))
	{
		printf("create rootImage error!!!\n");
		return;
	}

	// ������������0
	memset(buff, 0, 512);
	for(i=0; i < HEADS * SPT * CYLINDERS; i++)
	{
		fwrite(buff, 512, 1, fp);			// ò��fwriteһ��д��������̫�󣬻����!!! ��ԭ����δ֪�� :-(
	}

	// ���ó�����ṹ��ע�⣺��0�������ڵ��Ǹ������ڵ㣬��һ�����п��Ǹ������ڵ�����ݿ顣
	sup_b.size = HEADS * SPT * CYLINDERS;
	sup_b.nfreeblocks = sup_b.size - 2 - ((NR_D_INODE+512/sizeof(struct d_inode)-1) / (512 / sizeof(struct d_inode))) - 1;
	sup_b.nextfb = 0;
	sup_b.ninodes = NR_D_INODE;
	sup_b.nfreeinodes = NR_D_INODE-1;
	sup_b.nextfi = 0;
	pfreeblock = sup_b.size - sup_b.nfreeblocks;				// ��ʼ��pfreeblock
	// ���ó������е� ���п� ��
	for(i=0; i < NR_D_FBT && pfreeblock < sup_b.size; i++, pfreeblock++)
	{
		sup_b.fb_table[i] = pfreeblock;
	}
	// ���ó������е� ���������ڵ� ��
	for(i=0; i < NR_D_FIT && i < NR_D_INODE; i++)
	{
		sup_b.fi_table[i] = i+1;
	}
	sup_b.store_nexti = i+1;
	fseek(fp, 512L, SEEK_SET);									// ������������
	fwrite(&sup_b, sizeof(struct d_super_block), 1, fp);		// д������

	// ����ʣ����п�
	while(1)
	{
		fseek(fp, 512 * sup_b.fb_table[NR_D_FBT-1], SEEK_SET);// ���ϴε����һ�����д��̿���Ϊ���ӿ飬�洢���п�����
		for(i=0; i < NR_D_FBT && pfreeblock < sup_b.size; i++, pfreeblock++)// �������� ���п� ��
		{
			sup_b.fb_table[i] = pfreeblock;
		}
		if(i < NR_D_FBT)										// ���п鴦�����
		{
			sup_b.fb_table[i] = END_D_FB;						// END_D_FB ���п������־
			fwrite(&sup_b.fb_table, sizeof(unsigned long) * NR_D_FBT, 1, fp);
			break;
		}
		fwrite(&sup_b.fb_table, sizeof(unsigned long) * NR_D_FBT, 1, fp);
	}

	// ���������ڵ��
	/* do nothing but setup the root-inode, because that the all is 0 */
	/* NOTE! �������ڵ��޸��ڵ㣬����Ŀ¼��������'..' */
	root.mode |= FILE_DIR;
	root.mode |= FILE_RONLY;
	root.nlinks = 1;
	root.size = sizeof(struct file_dir_struct);
	root.zone[0] = sup_b.size - sup_b.nfreeblocks - 1;
	fseek(fp, 512 * 2, SEEK_SET);
	fwrite(&root, sizeof(struct d_inode), 1, fp);
	dir.nrinode = 0;
	strcpy(dir.name, ".");
	fseek(fp, 512 * root.zone[0], SEEK_SET);
	fwrite(&dir, sizeof(struct file_dir_struct), 1, fp);

	fclose(fp);
}

int main(void)
{
	mkfs();

	printf("===OK!\n");
	printf("Size of FS is %f MB.\n", HEADS * SPT * CYLINDERS * 512.0 / 1024.0 / 1024.0);
	printf("cylinders: %d, heads: %d, spt: %d.\n", CYLINDERS, HEADS, SPT);
	printf("###Please check 'Bochs' or 'VMware' configuration.\n");
	printf("mkfs exit.\n\n");

	return 0;
}