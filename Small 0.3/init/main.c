/*
 * Small/init/main.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.1 2013-01-30 21:48:27
 * V0.2 2013-02-01 15:36:45
 * V0.3 2013-02-21 13:10:21
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

#include "sys_set.h"
#include "string.h"
#include "console.h"
#include "keyboard.h"
#include "stdio.h"
#include "sched.h"
#include "kernel.h"
#include "memory.h"
#include "harddisk.h"
#include "file_system.h"

extern int system_call_22222(void);
extern int system_call_33333(void);
extern int system_call_44444(void);

char showmsg[] = "Small/init/main Start!\n!!\b\bMy printf() Start!\n";
int sec = 1;
char * proc1 = "this is process 1.\n";
char * proc2 = "this is process 2.\n";


int main(void)
{
	trap_init();
	console_init();
	keyboard_init();
	hd_init();
	mem_init();
	sched_init();
	buff_init();	// NOTE! 需在内存初始化之后调用
	inode_init();
	file_table_init();

	sti();

	// My printf()
	// 注意：printf("abc\n") 会被gcc替换为内置函数 puts("abc")。故，需在编译时需加上 -fno-builtin 选项，关闭GCC的内置函数替换功能。
//	printf(".............................\n");
//	printf("Hello %u!\n%sCopyright ZYF 2012%d\n", 2013, showmsg, -2013);
//	printf(".............................\n");
//	printf("=== OK.\n\n");
	
	set_idt(TRAP_R3, system_call_22222, 0x81);
	set_idt(TRAP_R3, system_call_33333, 0x82);
	set_idt(TRAP_R3, system_call_44444, 0x83);

	move_to_user_mode();

	if(!({int ret=-1; __asm__("int $0x80" :"=a"(ret) ::); ret;}))
	{
		if(!({int ret=-1; __asm__("int $0x80" :"=a"(ret) ::); ret;}))
		{		
			for(;;)
			{
			
				__asm__("int $0x83");

//				__asm__("int $0x82" ::"a"((sec++)%25+1) :);

				/*
				 * DEBUG_DELAY()是宏定义，其内容与debug_delay()相同，因为子进程与父进程还在共享用户栈，
				 * 故不能调用函数。
				 */
//				DEBUG_DELAY();
				/*
				 * well, 由于显存的段描述符特权级为0，处于用户态的程序代码无法访问。故，目前不执行该语句。
				 */
//				printf("this is process 2.\n");
//				__asm__("int $0x81" ::"a"(proc2) :);
			}
		}
		for(;;)
		{			
			/*
			 * DEBUG_DELAY()是宏定义，其内容与debug_delay()相同，因为子进程与父进程还在共享用户栈，
			 * 故不能调用函数。
			 */
//			DEBUG_DELAY();
			/*
			 * well, 由于显存的段描述符特权级为0，处于用户态的程序代码无法访问。故，目前不执行该语句。
			 */
//			printf("this is process 1.\n");
//			__asm__("int $0x81" ::"a"(proc1) :);
		}
	}

	for(;;)
	{
		/*
		 * DEBUG_DELAY()是宏定义，其内容与debug_delay()相同，因为子进程与父进程还在共享用户栈，
		 * 故不能调用函数。
		 */
//		DEBUG_DELAY();
		/*
		 * well, 由于显存的段描述符特权级为0，处于用户态的程序代码无法访问。故，目前不执行该语句。
		 */
//		printf("this is process 0.\n");
//		__asm__("int $0x81" ::"a"("this is process 0.\n") :);
	}

	return 0;
}