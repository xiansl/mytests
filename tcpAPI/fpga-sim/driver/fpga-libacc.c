#include <stdio.h>
#include <pthread.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <inttypes.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/time.h>

#include "fpga-libacc.h"




int __acc_open (struct acc_handler_t * acc_handler, int port_id) {
	int rc = -1;
	struct ioctl_acc_open acc_open_req;
	struct ioctl_acc_malloc acc_malloc_req;
	char * fpgacomnode = "/dev/fpgacom";
	char * fpganode = "/dev/fpga";
	int page_size = sysconf(_SC_PAGESIZE);

	if((acc_handler->fpgacomFp = open(fpgacomnode, O_RDWR)) < 0) {
		printf("Error:can not open the device: %s\n", fpgacomnode);
		return rc;
	}

	if (acc_handler->acc_name [0] == '0') {
		acc_open_req.by_acc_id = 1;
		acc_open_req.acc_id = strtoull (acc_handler->acc_name, NULL, 0);
		printf ("open ACC by id : 0x%llx\n", acc_open_req.acc_id);
	} else {
		acc_open_req.by_acc_id = 0;
		memcpy (acc_open_req.name, acc_handler->acc_name, ACC_NAME_SIZE);
	}
	acc_open_req.port_id = port_id;
	if (ioctl(acc_handler->fpgacomFp, IOCTL_ACC_OPEN, & acc_open_req)) {
		printf("Open ACC %s fail.\n", acc_handler->acc_name);
		close(acc_handler->fpgacomFp);
		acc_handler->fpgacomFp = -1;
		return rc;
	}

	if (acc_open_req.port_id < 0) {
		printf("Open %s fail. The reason is :\n", acc_handler->acc_name);
		switch (acc_open_req.error_flag) {
			case -1 : printf("\tCan not find the FPGA.\n"); break;
			case -2 : printf("\tThe ACC is not supported.\n"); break;
			case -3 : printf("\tACC dose not support specified port.\n"); break;
			case -4 : printf("\tACC port conflicts on the specified port.\n"); break;
			case -5 : printf("\tFPGA is too busy to open the ACC.\n"); break;
			case -6 : printf("\tFail to reconfigure the ACC.\n"); break;
			default : printf("\tUnknow reason.\n"); break;
		}
		close(acc_handler->fpgacomFp);
		acc_handler->fpgacomFp = -1;
		return rc;
	}
	acc_handler->acc_fd = acc_open_req.acc_fd;

	if((acc_handler->fpgaFp = open(fpganode, O_RDWR)) < 0) {
		printf("Error: can not open the device: %s\n", fpganode);
		close(acc_handler->fpgacomFp);
		acc_handler->fpgacomFp = -1;
		return rc;
	}

	acc_malloc_req.acc_fd = acc_handler->acc_fd;
	acc_malloc_req.data_size = acc_handler->data_buffer_size;
	acc_malloc_req.result_size = acc_handler->result_buffer_size;

	if (ioctl(acc_handler->fpgaFp, IOCTL_ACC_MALLOC, & acc_malloc_req)) {
		printf ("Too many users. The Descriptor Queue is full. ");
		printf ("Can not malloc buffers for ACC %s. ", acc_open_req.name);
		printf ("Please try again later.\n");
		close(acc_handler->fpgacomFp);
		acc_handler->fpgacomFp = -1;
		return rc;
	}
	acc_handler->des_key = acc_malloc_req.key;

	rc = 0;
	errno = 0;
	acc_handler->cmd_page = (unsigned int *)mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, acc_handler->fpgacomFp, 0);
	if(errno) {
		printf("mmap pci bar error. %s\n", strerror(errno));
		acc_handler->cmd_page = NULL;
		rc = -1;
	}

	acc_handler->data_buffer = (void *)mmap(NULL, acc_handler->data_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, acc_handler->fpgaFp, 0);
	if(errno) {
		printf("mmap dma data buffer error. %s\n", strerror(errno));
		acc_handler->data_buffer = NULL;
		rc = -1;
	}

	acc_handler->result_buffer = (void *)mmap(NULL, acc_handler->result_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, acc_handler->fpgaFp, 0);
	if(errno) {
		printf("mmap dma data buffer error. %s\n", strerror(errno));
		acc_handler->result_buffer = NULL;
		rc = -1;
	}

	acc_handler->des = (struct acc_descriptor *)mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, acc_handler->fpgaFp, 0);
	if(errno) {
		printf("mmap dma data buffer error. %s\n", strerror(errno));
		acc_handler->des = NULL;
		rc = -1;
	}

	if (rc < 0)
		close(acc_handler->fpgacomFp);
	else
		acc_handler->des->priority = 0x3344;

	return rc;
}

void __acc_close(struct acc_handler_t * acc_handler) {

	unsigned long long acc_fd;
	int page_size = sysconf(_SC_PAGESIZE);

	if (acc_handler->cmd_page)
		munmap((void *)acc_handler->cmd_page, page_size);
	if (acc_handler->data_buffer)
		munmap((void *)acc_handler->data_buffer, acc_handler->data_buffer_size);
	if (acc_handler->result_buffer)
		munmap((void *)acc_handler->result_buffer, acc_handler->result_buffer_size);
	if (acc_handler->des)
		munmap((void *)acc_handler->des, page_size);

	if (acc_handler->des_key)
		if (ioctl(acc_handler->fpgaFp, IOCTL_ACC_FREE, &acc_handler->des_key))
			printf ("Warning. Can not normally free ACC buffers.\n");

	acc_fd = acc_handler->acc_fd;
	if (acc_handler->fpgacomFp >= 0)
		if(ioctl(acc_handler->fpgacomFp, IOCTL_ACC_CLOSE, &acc_fd)) {
			printf("Close %s fail.\n", acc_handler->acc_name);
			return;
		}

	close(acc_handler->fpgaFp);
	close(acc_handler->fpgacomFp);
}


long __acc_do_job(struct acc_handler_t * acc_handler) {
	int j;
	volatile unsigned int * ctl_page;
	volatile struct acc_descriptor * descriptor;
	struct ioctl_acc_wdes acc_des;
	unsigned int dma_result_tail, result_status;
	unsigned long long begin_cycle, end_cycle;


	ctl_page = acc_handler->cmd_page;
	acc_des.key = acc_handler->des_key;
	acc_des.ret = 0;

	descriptor = acc_handler->des;
	descriptor->status = 0ULL;

	do {
		if(ioctl(acc_handler->fpgacomFp, IOCTL_ACC_WDES, &acc_des)) {
			printf ("Send descriptor to kernel error.\n");
			return -1;
		}

	} while (0 == acc_des.ret);

	while (1) {
		result_status = ((unsigned int)descriptor->status);
		if (result_status)
			break;
		else
			for (j = 0; j < 100; j ++)	;
		
	}

	return result_status;
}


void __job_init (struct acc_handler_t * acc_handler, const char * acc_name, unsigned int input_buf_size, unsigned int output_buf_size)
{
	int page_size = sysconf(_SC_PAGESIZE);

	strcpy(acc_handler->acc_name, acc_name);
	acc_handler->time_out_counter = 2400000000UL;
	acc_handler->data_buffer_size = ((input_buf_size/page_size) +((input_buf_size%page_size)?1:0))*page_size;
	acc_handler->result_buffer_size = ((output_buf_size/page_size) +((output_buf_size%page_size)?1:0))*page_size;
	acc_handler->cmd_page = NULL;
	acc_handler->data_buffer = NULL;
	acc_handler->result_buffer = NULL;
	acc_handler->des = NULL;
	acc_handler->des_key = 0;
}


void * pri_acc_open(struct acc_handler_t * acc_handler, const char * acc_name, unsigned int in_buf_size, unsigned int out_buf_size, int port_id)
{
	int rc;
	__job_init (acc_handler, acc_name, in_buf_size, out_buf_size);
	
	rc = __acc_open(acc_handler, port_id);
	if (rc != 0)
		return NULL;
	else
		return acc_handler->data_buffer;
}


void * acc_open(struct acc_handler_t * acc_handler, const char * acc_name, unsigned int in_buf_size, unsigned int out_buf_size)
{
	int rc;
	__job_init (acc_handler, acc_name, in_buf_size, out_buf_size);
	
	rc = __acc_open(acc_handler, -1);
	if (rc != 0)
		return NULL;
	else
		return acc_handler->data_buffer;
}


long acc_do_job (struct acc_handler_t *acc_handler, const char * param, unsigned int job_len, void ** result_buf)
{
	long rc;

	__job_set_param (acc_handler, param);
	__job_set_data_len (acc_handler, job_len);
	* result_buf = acc_handler->result_buffer;
	rc = __acc_do_job(acc_handler);

	if (rc == 1)
		rc = -2;
	else if (rc == 2)
		rc = -3;
	else if (rc > 0)
		rc = rc&0xfffffffffffffffc;
	return rc;
}


void acc_close(struct acc_handler_t * acc_handler)
{
	__acc_close(acc_handler);
}
