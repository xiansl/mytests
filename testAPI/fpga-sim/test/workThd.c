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
#include <sched.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/time.h>

#include "workThd.h"
#include "../driver/fpga-libacc.h"

#ifdef LOCAL_TSC

#ifndef __USE_GNU
#define __USE_GNU 1
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


#define CONFIG_X86_64 1

#ifdef CONFIG_X86_64
#define DECLARE_ARGS(val, low, high)	unsigned low, high
#define EAX_EDX_VAL(val, low, high)	((low) | ((unsigned long long)(high) << 32))
#define EAX_EDX_ARGS(val, low, high)	"a" (low), "d" (high)
#define EAX_EDX_RET(val, low, high)	"=a" (low), "=d" (high)
#else
#define DECLARE_ARGS(val, low, high)	unsigned long long val
#define EAX_EDX_VAL(val, low, high)	(val)
#define EAX_EDX_ARGS(val, low, high)	"A" (val)
#define EAX_EDX_RET(val, low, high)	"=A" (val)
#endif


static __always_inline unsigned long long native_read_tsc(void)
{
	DECLARE_ARGS(val, low, high);

	asm volatile("rdtsc" : EAX_EDX_RET(val, low, high));

	return EAX_EDX_VAL(val, low, high);
}


uint64_t * local_job_cycles;
uint64_t * job_cycles;
uint64_t * job_cycles0;
uint64_t * job_cycles1;
#endif

int loop_sum;
uint64_t finish_num = 0;

//	Here is the sample that how software do a job with accelerator

int do_my_work(struct acc_handler_t * acc_handler, void * in_buffer) {

	long ret;
	unsigned int job_len;
	void * result_buffer;
#ifdef LOCAL_TSC
	unsigned long long begin_cycle, end_cycle;
#endif
	char param [16];

//	Here you set a software-accelerator co-defined parameter
	param [0] = 0x23;

//	Here you copy your source data into this buffer
//	Copy "your data" into "acc_handler->data_buffer"
//	memset (acc_handler->data_buffer, 0, 4096);

//	Here you set the real data length in the source data buffer
//	so that accelerator know the real data length in the buffer
	job_len = acc_handler->data_buffer_size;

#ifdef LOCAL_TSC
	begin_cycle = native_read_tsc();
#endif
//	Here the software sends the job to accelerator and wait completion
	ret = acc_do_job (acc_handler, param, job_len, &result_buffer);

	if (ret > 0) {
	//	Do your work with the result here
		finish_num ++;

#ifdef LOCAL_TSC
		if (loop_sum < 102400) {
			end_cycle = native_read_tsc();
			local_job_cycles [loop_sum] = end_cycle - begin_cycle;
		}
#endif

	//	Here you can read the result out of the result buffer
	//	ptr = (char *) acc_handler->result_buffer;

	//	Here is some sample code to print out the result.
	//	printf ("\nTest complete. Result :\t%02x_%02x_%02x_%02x\n\n", ptr [0], ptr [1], ptr [2], ptr [3]);
	} else if (ret == -2)
		printf ("Warning : Job is reset.\n");
	else
		printf ("Error : Job Time out.\n");

	return ret; 
}
/*************************************************/



int run = 2;
int ack = 0;

int job_send = 0;
uint64_t job_usec;
uint64_t pay_load = 0;
int job_num = 0;

void *worker_thread(void *data) {

	int ret, iters;
	WorkThdData* myData = (WorkThdData *)data;
	void * in_buffer;
	struct timeval start, end, dt, block_start, block_end;
	struct acc_handler_t acc_handler;
	uint64_t size_sum, random_step;
	uint64_t interval, base_time;
	cpu_set_t cpu_mask;

	ret = 0;
	size_sum = 0;
	loop_sum = 0;

	iters = myData->iters;

	if (myData->cpu_set > 0) {
		CPU_ZERO (&cpu_mask);
		CPU_SET (myData->cpu_set, &cpu_mask);
		if (-1 == sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask)) {
			printf ("Set Affinity to CPU %d fail.\n", myData->cpu_set);
			goto done;
		}
	}

	if (myData->random_iters > 0)
	{
		interval = 1000000/myData->random_iters;
		srand(time(0));
		base_time = 0;
	}
	else
		interval = 0;

	random_step = interval;

#ifdef LOCAL_TSC
	job_cycles0 = (uint64_t *) malloc (102400*sizeof(uint64_t));
	if (job_cycles0 == NULL)
		goto done;
	job_cycles1 = (uint64_t *) malloc (102400*sizeof(uint64_t));
	if (job_cycles1 == NULL) {
		free (job_cycles0);
		goto done;
	}
	local_job_cycles = job_cycles0;
#endif


	// Open the accelerator
	in_buffer = acc_open(&acc_handler, myData->acc_name, myData->input_size*sysconf(_SC_PAGESIZE), myData->input_size*sysconf(_SC_PAGESIZE));

	if (in_buffer != NULL) {
		gettimeofday(&start, NULL);
		gettimeofday(&block_start, NULL);
		while ((run == 2) && (iters != 0))
		{
			gettimeofday(&end, NULL);
			timersub(&end, &start, &dt);
			if (end.tv_sec != start.tv_sec)
			{
				job_usec = dt.tv_usec + 1000000 * dt.tv_sec;
				pay_load = size_sum;
				job_num  = loop_sum;
#ifdef LOCAL_TSC
				if (local_job_cycles == job_cycles0) {
					local_job_cycles = job_cycles1;
					job_cycles = job_cycles0;
				} else {
					local_job_cycles = job_cycles0;
					job_cycles = job_cycles1;
				}
#endif
				random_step = interval;
				base_time = 0;
				start = end;
				size_sum = 0;
				loop_sum = 0;
				job_send = 1;
			}

			if (interval > 0) {
				if ((dt.tv_usec + 1000000 * dt.tv_sec) < random_step)
					continue;
				else {
					base_time += interval;
					random_step = base_time + (rand()%interval);
				}
			}

			// Here is the real place where we send job to FPGA
			ret = do_my_work (&acc_handler, in_buffer);
			if (-1 == ret)
				break;

			size_sum += acc_handler.data_buffer_size;
			loop_sum ++;

			if (iters > 0)
				iters --;
		}
		gettimeofday(&block_end, NULL);
	}

#ifdef LOCAL_TSC
	free (job_cycles0);
	free (job_cycles1);
#endif
	// Here we close the accelerator
	acc_close(&acc_handler);

	if (myData->iters > 0) {
		timersub(&block_end, &block_start, &dt);
		printf ("%s Complete totall test. %ld sec + %ld usec.\n", myData->acc_name, dt.tv_sec, dt.tv_usec);
	}
done:
	ack = 1;
	pthread_exit(NULL);
}
