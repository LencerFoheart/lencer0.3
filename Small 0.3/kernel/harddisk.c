/*
 * Small/kernel/harddisk.c
 * 
 * (C) 2012-2013 Yafei Zheng
 * V0.3 2013-2-26 11:27:03
 *
 * Email: e9999e@163.com, QQ: 1039332004
 */

//#ifndef __DEBUG__
//#define __DEBUG__
//#endif

#include "harddisk.h"
#include "sched.h"
#include "sys_set.h"
#include "sys_nr.h"
#include "kernel.h"
#include "types.h"
#include "string.h"
#include "stdio.h"

extern int hd_int(void);


void (*do_hd)(void) = NULL;						// Ӳ���жϺ���ָ��

struct hd_struct hd = {0};						// Ӳ�̲�����ṹ
struct hd_request request[NR_HD_REQUEST] = {{0,},};	// Ӳ�����������
int rear = -1, head = -1, req_count = 0;		// Ӳ�����������β,ͷ,����

/*
 * ����Ӳ���������������䷢�����
 * NOTE! ������:��1��ʼ!!!!!! ��������:0��1, ��ͷ��:��0��ʼ, �����:��0��ʼ.
 */
void request_hd_drv(struct hd_request *q)
{
	if(BUSY_STAT & in_b(HD_STATE))
	{
		int tmp = 10000;
		while(tmp-- && (BUSY_STAT & in_b(HD_STATE)))
			;
		if(BUSY_STAT & in_b(HD_STATE))
		{
			panic("request_hd_drv: the controller is always busy.");
		}
	}
	if(!(READY_STAT & in_b(HD_STATE)))
	{
		int tmp = 10000;
		while(tmp-- && !(READY_STAT & in_b(HD_STATE)))
			;
		if(!(READY_STAT & in_b(HD_STATE)))
		{
			panic("request_hd_drv: the drive isn't ready.");
		}
	}
	do_hd = (*q).int_addr;
	out_b(HD_CTRL, hd.ctrl);
	out_b(HD_PRECOMP, (hd.pre_w_cyl>>2) & 0xff);		// �����4!!!!
	out_b(HD_SECTORS, (*q).sectors & 0xff);
	out_b(HD_NRSECTOR, (*q).nrsector & 0xff);
	out_b(HD_LCYL, (*q).nrcyl & 0xff);
	out_b(HD_HCYL, ((*q).nrcyl >> 8) & 0x3);
	out_b(HD_DRV_HEAD, ((0x5<<5) | (((*q).nrdrv&0x1)<<4) | ((*q).nrhead&0xf)) & 0xff);
	out_b(HD_COMMAND, (*q).cmd & 0xff);
}

void hd_read_int(void)
{
	if(ERR_STAT & in_b(HD_STATE))
	{
		printf("Kernel MSG: hd_read_int: read harddisk ERROR, error code: %x.\n", in_b(HD_ERROR));
		return;
	}
	__asm__("cld;rep;insw" ::"c"(256), "D"((request[head].bh)->p_data), "d"(HD_DATA) :);
	if(--(request[head].sectors))
	{
		do_hd = hd_read_int;
		/* nothing at present */
		return;
	}
	--req_count;
	wake_up(&request[head].wait);
	do_hd_request();
}

void hd_write_int(void)
{
	if(ERR_STAT & in_b(HD_STATE))
	{
		printf("Kernel MSG: hd_write_int: write harddisk ERROR, error code: %x.\n", in_b(HD_ERROR));
		return;
	}
	__asm__("cld;rep;outsw" ::"c"(256), "S"((request[head].bh)->p_data), "d"(HD_DATA) :);
	if(--(request[head].sectors))
	{
		do_hd = hd_write_int;
		/* nothing at present */
		return;
	}
	--req_count;
	wake_up(&request[head].wait);
	do_hd_request();
}

void hd_default_int(void)
{
	panic("hd_default_int: bad harddisk interrupt.");
}

/*
 * ����Ӳ�����������Ϊ��������ָ�룬������������߼�������
 */
void insert_hd_request(struct buffer_head * bh, unsigned int cmd, unsigned int sectors, unsigned int nr_v_sector)
{
	d_printf("insert_hd_request: sectors=%u, nr_v_sector=%u, %s, total_sectors:%u.\n",
		sectors, nr_v_sector, (cmd==WIN_READ ? ("READ") : ("WRITE")), hd.cyls * hd.heads * hd.sectors);

	if(!bh || sectors<0 || nr_v_sector<0 || (nr_v_sector+sectors)>=(hd.cyls * hd.heads * hd.sectors))
	{
		panic("insert_hd_request: bad parameters.");
	}
	if(NR_HD_REQUEST == req_count)	// ������
	{
		panic("insert_hd_request: the hd-request-queue is full.");
	}
	cli();
	++req_count;
	rear = (++rear) % NR_HD_REQUEST;
	sti();
	request[rear].wait = NULL;		// NOTE! that's right
	request[rear].bh = bh;
	switch(cmd)
	{
	case WIN_READ:
		request[rear].int_addr = hd_read_int; break;
	case WIN_WRITE:
		request[rear].int_addr = hd_write_int; break;
		/* ��δ���� */
	default:
		panic("insert_hd_request: bad hd-cmd."); break;
	}
	request[rear].cmd = cmd;
	request[rear].sectors = sectors;
	request[rear].nrdrv = 0;	// Ĭ��Ϊ0
	request[rear].nrsector = (nr_v_sector % hd.sectors) + 1;	// ������:��1��ʼ!!!!!!
	request[rear].nrcyl = (nr_v_sector / hd.sectors) / hd.heads;
	request[rear].nrhead = (nr_v_sector / hd.sectors) % hd.heads;

	/*
	 * ������֮ǰ��û������������do_hd_request��ִ��Ӳ��������ִ��Ӳ������֮ǰ�ر��жϣ�
	 * ִ��Ӳ������֮�󽫵�ǰ����˯�ߣ�Ȼ�󵱵�ǰ���̱�����ʱ�����жϡ�NOTE! ��ǰ���̿����ж�
	 * ������Ӱ�쵽�������̣���Ϊÿ�����̶����Լ���tss��Ӳ�̿������ж���������ڵ�������������
	 * ʱ����Ӧ��
	 */
	if(1 == req_count)
	{
        cli();
        do_hd_request();
		sleep_on(&request[rear].wait, INTERRUPTIBLE);
		sti();
	}
	else
	{
        sleep_on(&request[rear].wait, INTERRUPTIBLE);
	}
}

/*
 * ִ��Ӳ������
 */
void do_hd_request(void)
{
	if(req_count)
	{		
		head = (++head) % NR_HD_REQUEST;
		request_hd_drv(&request[head]);
	}
}

void hd_init(void)
{
	do_hd = hd_default_int;

	/*
	 * NOTE! �����ֽڶ��룬��������Ǵ����!!! Ӧ��һ��һ����ֵ��
	 * �������ڴ˴����󣬵���Ӳ�̶�ȡ����!!! ��Ϊ�ᵼ��дǰԤ��������Ŵ���
	 */
//	memcpy(&hd, (void *)0x90004, sizeof(struct hd_struct));	// ����!!!
//	printf("sizeof(struct hd_struct) = %d\n", sizeof(struct hd_struct));
	hd.cyls				= *((unsigned short*)(0x90004 + 0));
	hd.heads			= *((unsigned char*)(0x90004 + 2));
	hd.w_small_elec_cyl = *((unsigned short*)(0x90004 + 3));
	hd.pre_w_cyl		= *((unsigned short*)(0x90004 + 5));
	hd.max_ECC			= *((unsigned char*)(0x90004 + 7));
	hd.ctrl				= *((unsigned char*)(0x90004 + 8));
	hd.n_time_limit		= *((unsigned char*)(0x90004 + 9));
	hd.f_time_limit		= *((unsigned char*)(0x90004 + 10));
	hd.c_time_limit		= *((unsigned char*)(0x90004 + 11));
	hd.head_on_cyl		= *((unsigned short*)(0x90004 + 12));
	hd.sectors			= *((unsigned char*)(0x90004 + 14));
	hd.none				= *((unsigned char*)(0x90004 + 15));
//	printf(" hd.ctrl=%u\n hd.pre_w_cyl=%u\n hd.cyls=%u\n hd.heads=%u\n hd.sectors=%u\n", hd.ctrl, hd.pre_w_cyl, hd.cyls, hd.heads, hd.sectors);

	zeromem(request, NR_HD_REQUEST * sizeof(struct hd_request));
	req_count = 0;

	set_idt(INT_R0, hd_int, NR_HD_INT);
}