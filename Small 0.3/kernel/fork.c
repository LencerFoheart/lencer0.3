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
 * forkϵͳ���á�NOTE! ���������copy_process()ͬ����forkֻ����һ���½��̣��ӽ�����
 * ��������ʱ����ҳ����ֱ�������̻��ӽ���writeʱ���ӽ����ٷ����Լ���ҳ���Լ�ҳ�档
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
 * ���ƽ��̽ṹ��NOTE! ���������sys_fork()ͬ��������˵���μ�sys_fork().
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

	// ���ƽ��̽ṹ
	memcpy(p, current, sizeof(struct proc_struct));
	// �����½��̽ṹ
	p->state = UNINTERRUPTIBLE;
	p->counter = p->priority;
	p->pid = last_pid;
	p->fpid = current->pid;
	p->utime = 0;
	p->ktime = 0;
	p->cutime = 0;
	p->cktime = 0;
	/* NOTE! �ӽ����븸���̵��û�̬�����ַ�ռ���ͬ������3GB-4GB����������� */
	// �����½���tss�ṹ
	p->tss.back_link = NULL;
	p->tss.esp0 = (unsigned long)p + PAGE_SIZE;
	p->tss.eip = eip;
	p->tss.eflags = eflags;
	p->tss.eax = 0;		// NOTE! fork���õ��ӽ��̷���ֵΪ0
	p->tss.ebx = ebx;
	p->tss.ecx = ecx;
	p->tss.edx = edx;
	p->tss.esp = esp;
	p->tss.ebp = ebp;
	p->tss.esi = esi;
	p->tss.edi = edi;
	p->tss.es = es & 0xffff;	// ��16λ��Ч
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
 * �ڽ��̲��������ս��̣�����ɹ�����Ϊ�½���׼��pid��
 * 
 * NOTE! �ú������������̲۵�0�����0��pid�̶�Ϊ0��
 *
 * ���û�пս��̣�����0��
 */
int find_empty_process(void)
{
	int nr = NR_PROC - 1;
	static char isfirstcalled = 1;	// �ú����Ƿ��һ�α�����
	int i = 0;

	if(isfirstcalled)			// ��һ�ε������ʼ��last_pid����ΪΪ0��ȫ�ֱ������ʼ����
	{
		last_pid = 0;
		isfirstcalled = 0;
	}

	for(; nr; nr--)
	{
		if(!proc[nr])
		{
			while(1)	// ׼��pid
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
	// �޿ս���
	return 0;
}