/*
 * Small/kernel/debug.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-2-28 11:10:08
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

/*
 * 此文件用于在bochs中调试内核。
 */

//#ifndef __DEBUG__
//#define __DEBUG__
//#endif

#include "kernel.h"
#include "file_system.h"
#include "sched.h"
#include "stdio.h"
#include "string.h"

void debug_delay(void)
{
	for(int i=0; i<DELAY_COUNT; i++)
		for(int j=0; j<10000; j++)
			;
}

void no_printf(char * fmt, ...)
{
	/* nothing, it's right */
}

void creat_write_read_close_file(void)
{
	static int i = 1;
	char buff_in[100] = "Hello world! \nMy name is ZYF, I'm writing a small OS. \nThanks.";
	char buff_out[100] = {0};
	char filename[50] = {0};

	/* 已创建了内核默认的最多磁盘存储文件数，则返回 */
	if(i >= NR_D_INODE)
	{
		return;
	}

	if(1 == i)
	{
		/*
		 * NOTE! 我们在此初始化内存超级块。
		 */
		super_init();
		current->root_dir = current->current_dir = iget(0, 0);
		UNLOCK_INODE(current->root_dir);	// 解锁
		d_printf("Get root-inode return.\n");
		d_printf("sizeof(struct d_inode) = %d.\n", sizeof(struct d_inode));
	}
	d_printf("inode-size: %d, nrinode: %d, zone[0]: %d.\n",
		current->root_dir->size, current->root_dir->nrinode, current->root_dir->zone[0]);


	printf("This is the content will be written: \n%s\n\n", buff_in);


	sprintf(filename, "/hello--%d.txt", i++);
	printf("%s\n", filename);
	/* 创建文件并写入 */
	long desc = creat(filename, FILE_FILE | FILE_RW);
	if(-1 == desc)
	{
		printf("Create file which named '%s' failed!\n", filename);
		return;
	}
	d_printf("Create file return.\n");
	if(-1 == lseek(desc, 9959260, SEEK_SET))
	{
		printf("Lseek file which named '%s' failed!\n", filename);
		return;
	}
	d_printf("Lseek file return.\n");
	if(-1 == write(desc, buff_in, 1 + strlen(buff_in)))
	{
		printf("Write file which named '%s' failed!\n", filename);
		return;
	}
	d_printf("Write file return.\n");
	if(-1 == close(desc))
	{
		printf("Close file which named '%s' failed!\n", filename);
		return;
	}
	d_printf("Close file return.\n");


	/* 打印根目录含有的文件，NOTE! 目录是只读的 */
	long size = 0;
	if(-1 == (desc = open("/", O_RONLY)))
	{
		printf("Open file which named '%s' failed!\n", "/");
		return;
	}
	d_printf("Open file return.\n");
	size = lseek(desc, 0, SEEK_END);
	lseek(desc, 0, SEEK_SET);
	// NOTE! 此处做法欠妥! 因为目录项是按每块整数个存储的
	printf("The content of root-dir:\n");
	for(int j=0; j<size; j+=sizeof(struct file_dir_struct))
	{
		if(-1 == read(desc, buff_out, sizeof(struct file_dir_struct)))
		{
			printf("Read file which named '%s' failed!\n", "/");
			return;
		}
		d_printf("Read file return.\n");	
		printf(" '%s', ", ((struct file_dir_struct *)buff_out)->name);
	}
	printf("\n\n");
	if(-1 == close(desc))
	{
		printf("Close file which named '%s' failed!\n", "/");
		return;
	}
	d_printf("Close file return.\n");


	/* 打开之前创建的文件，读取 */
	if(-1 == (desc = open(filename, O_RW)))
	{
		printf("Open file which named '%s' failed!\n", filename);
		return;
	}
	d_printf("Open file return.\n");
	if(-1 == lseek(desc, 9959260, SEEK_SET))
	{
		printf("Lseek file which named '%s' failed!\n", filename);
		return;
	}
	d_printf("Lseek file return.\n");
	if(-1 == read(desc, buff_out, 1 + strlen(buff_in)))
	{
		printf("Read file which named '%s' failed!\n", filename);
		return;
	}
	d_printf("Read file return.\n");	
	if(-1 == close(desc))
	{
		printf("Close file which named '%s' failed!\n", filename);
		return;
	}
	d_printf("Close file return.\n");


	printf("This is the content of READ file: \n%s\n\n", buff_out);
}
