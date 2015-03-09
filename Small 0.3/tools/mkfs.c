/*
 * Small/tools/mkfs.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-02-18 23:58:43
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * 本文件包含文件系统生成程序。此处创建文件作为磁盘镜像来模拟磁盘。
 * 该文件系统类似于UNIX system V的文件系统。内核默认索引节点号从0开始。
 */

#include <stdio.h>						// 标准头文件，非自建
#include <string.h>						// 标准头文件，非自建
#include "../include/file_system.h"		// 包含文件系统的创建参数

#define DISK_SIZE		10				// 用于构建文件系统的磁盘的大小，MB。NOTE! 此处并不是真实的尺寸。

/*
 * NOTE! 正是由于 DISK_SIZE 不一定使生成的硬盘镜像，与真实的硬盘相匹配，才导致了错误。
 * 对于硬盘，磁头数、每磁道扇区数、磁道数均应是整数。故需以下定义。
 *
 * 在PC标准中有一个很有趣的现象，为了兼容大多数老的BIOS和驱动器，
 * 驱动器通常执行一个内部转译，转译为逻辑上每磁道63个扇区。因此，
 * 这里我们将磁道扇区数设置为63，以便兼容VMware等(注: Bochs不设置
 * 为63也是可以的)。
 */
#define HEADS			2				// 磁盘默认的磁头数
#define SPT				63				// 磁盘默认的每磁道扇区数，别修改它
#define CYLINDERS		(DISK_SIZE * 1024 * 1024 / 512 / HEADS / SPT)	// 磁盘磁道数

void mkfs(void)
{
	static char canrun = 1;				// 为1时，该函数可以执行
	FILE * fp = NULL;
	unsigned long i=0;					// 循环控制变量
	unsigned long pfreeblock = 0;		// 磁盘中下一个需处理的空闲块的块号
	char buff[512] = {0};
	struct d_super_block sup_b = {0};	// 超级块
	struct d_inode root = {0};			// 根索引节点
	struct file_dir_struct dir = {0};	// 根索引节点的目录表项

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

	// 将磁盘内容清0
	memset(buff, 0, 512);
	for(i=0; i < HEADS * SPT * CYLINDERS; i++)
	{
		fwrite(buff, 512, 1, fp);			// 貌似fwrite一次写的数据量太大，会出错!!! 但原因尚未知否 :-(
	}

	// 设置超级块结构。注意：第0号索引节点是根索引节点，第一个空闲块是根索引节点的数据块。
	sup_b.size = HEADS * SPT * CYLINDERS;
	sup_b.nfreeblocks = sup_b.size - 2 - ((NR_D_INODE+512/sizeof(struct d_inode)-1) / (512 / sizeof(struct d_inode))) - 1;
	sup_b.nextfb = 0;
	sup_b.ninodes = NR_D_INODE;
	sup_b.nfreeinodes = NR_D_INODE-1;
	sup_b.nextfi = 0;
	pfreeblock = sup_b.size - sup_b.nfreeblocks;				// 初始化pfreeblock
	// 设置超级块中的 空闲块 表
	for(i=0; i < NR_D_FBT && pfreeblock < sup_b.size; i++, pfreeblock++)
	{
		sup_b.fb_table[i] = pfreeblock;
	}
	// 设置超级块中的 空闲索引节点 表
	for(i=0; i < NR_D_FIT && i < NR_D_INODE; i++)
	{
		sup_b.fi_table[i] = i+1;
	}
	sup_b.store_nexti = i+1;
	fseek(fp, 512L, SEEK_SET);									// 跳过引导扇区
	fwrite(&sup_b, sizeof(struct d_super_block), 1, fp);		// 写超级块

	// 处理剩余空闲块
	while(1)
	{
		fseek(fp, 512 * sup_b.fb_table[NR_D_FBT-1], SEEK_SET);// 将上次的最后一个空闲磁盘块作为链接块，存储空闲块数组
		for(i=0; i < NR_D_FBT && pfreeblock < sup_b.size; i++, pfreeblock++)// 继续设置 空闲块 表
		{
			sup_b.fb_table[i] = pfreeblock;
		}
		if(i < NR_D_FBT)										// 空闲块处理完毕
		{
			sup_b.fb_table[i] = END_D_FB;						// END_D_FB 空闲块结束标志
			fwrite(&sup_b.fb_table, sizeof(unsigned long) * NR_D_FBT, 1, fp);
			break;
		}
		fwrite(&sup_b.fb_table, sizeof(unsigned long) * NR_D_FBT, 1, fp);
	}

	// 处理索引节点表
	/* do nothing but setup the root-inode, because that the all is 0 */
	/* NOTE! 根索引节点无父节点，故其目录表项中无'..' */
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