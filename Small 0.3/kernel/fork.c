/*
 * Small/kernel/fork.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-02-23 11:54:09
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

#include "sched.h"
#include "memory.h"
#include "kernel.h"
#include "stdio.h"
#include "string.h"
#include "types.h"

long last_pid = 0;

/*
 * fork系统调用。NOTE! 其参数须与copy_process()同步。fork只创建一个新进程，子进程与
 * 父进程暂时共享页表。直到父进程或子进程write时，子进程再分配自己的页表以及页面。
 */
int sys_fork(long none, long gs, long fs, long es, long ds, 
			 long edi, long esi, long ebp, long edx, long ecx, long ebx,
			 long eip, long cs, long eflags, long esp, long ss)
{
	int nr = 0;

	if(!(nr = find_empty_process()))
	{
		printf("Kernel MSG: fork: have no empty process!\n");
		return -1;
	}
	if(0 == (proc[nr] = (struct proc_struct *)get_free_page()))
	{
		printf("Kernel MSG: fork: have no free page!\n");
		return -1;
	}
	
	copy_process(nr, gs, fs, es, ds, 
		edi, esi, ebp, edx, ecx, ebx,
		eip, cs, eflags, esp, ss);
	proc[nr]->state = RUNNING;
	return last_pid;
}

/*
 * 复制进程结构。NOTE! 其参数须与sys_fork()同步。其他说明参见sys_fork().
 */
void copy_process(int nr, long gs, long fs, long es, long ds, 
				long edi, long esi, long ebp, long edx, long ecx, long ebx,
				long eip, long cs, long eflags, long esp, long ss)
{
	if(nr < 0 || nr >= NR_PROC)
	{
		panic("copy_process: nr of process is too small or big.");
	}
	if(0 == nr)
	{
		panic("copy_process: trying to alter the proc_struct of process 0.");
	}

	struct proc_struct * p = proc[nr];

	// 复制进程结构
	memcpy(p, current, sizeof(struct proc_struct));
	// 调整新进程结构
	p->state = UNINTERRUPTIBLE;
	p->counter = p->priority;
	p->pid = last_pid;
	p->fpid = current->pid;
	p->utime = 0;
	p->ktime = 0;
	p->cutime = 0;
	p->cktime = 0;
	/* NOTE! 子进程与父进程的用户态虚拟地址空间相同，都是3GB-4GB，故无需调整 */
	// 调整新进程tss结构
	p->tss.back_link = NULL;
	p->tss.esp0 = (unsigned long)p + PAGE_SIZE;
	p->tss.eip = eip;
	p->tss.eflags = eflags;
	p->tss.eax = 0;		// NOTE! fork调用的子进程返回值为0
	p->tss.ebx = ebx;
	p->tss.ecx = ecx;
	p->tss.edx = edx;
	p->tss.esp = esp;
	p->tss.ebp = ebp;
	p->tss.esi = esi;
	p->tss.edi = edi;
	p->tss.es = es & 0xffff;	// 低16位有效
	p->tss.cs = cs & 0xffff;
	p->tss.ss = ss & 0xffff;
	p->tss.ds = ds & 0xffff;
	p->tss.fs = fs & 0xffff;
	p->tss.gs = gs & 0xffff;
	p->tss.ldt = _LDT(nr);
	p->tss.trace_bitmap = 0x80000000;

	clone_page_tables(&(p->tss.cr3));
	set_ldt_desc(nr, &(p->ldt));
	set_tss_desc(nr, &(p->tss));
}

/*
 * 在进程槽中搜索空进程，如果成功，则为新进程准备pid。
 * 
 * NOTE! 该函数不搜索进程槽第0项，进程0的pid固定为0。
 *
 * 如果没有空进程，返回0。
 */
int find_empty_process(void)
{
	int nr = NR_PROC - 1;
	static char isfirstcalled = 1;	// 该函数是否第一次被调用
	int i = 0;

	if(isfirstcalled)			// 第一次调用需初始化last_pid，因为为0的全局变量需初始化。
	{
		last_pid = 0;
		isfirstcalled = 0;
	}

	for(; nr; nr--)
	{
		if(!proc[nr])
		{
			while(1)	// 准备pid
			{
				(++last_pid < 0) ? last_pid=1 : 0;
				for(i=NR_PROC-1; i; i--)
				{
					if(last_pid == proc[i]->pid)
					{
						break;
					}
				}
				if(0 == i)
				{
					break;
				}
			}
			return nr;
		}
	}
	// 无空进程
	return 0;
}