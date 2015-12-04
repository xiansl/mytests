#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include "../driver/fpga-libacc.h"




int main (int argc, char **argv) {

	long ret, iters;
	int count, section_id;
	struct acc_handler_t acc_handler;
	char * acc_name;
	unsigned char *ptr;
	void * in_buffer, *result_buffer;
	unsigned int real_in_size, real_out_size, in_buffer_size, out_buffer_size;
	struct timeval open_start, open_end, open_dt;
	struct timeval exe_start, exe_end, exe_dt;
	char param [16];

	param [0] = 0x24;
	ret = 0;
	section_id = 3;

	if (argc != 3) {
		printf ("Usage : micro_test ACC_name/ACC_ID /Workload\n");
		printf ("\tworkload must be a mutiple of 4 K bytes\n");
		printf ("\tExample : micro_test AES 1 (means execute AES with workloads of 1* 4K bytes)\n");
		printf ("\tExample : micro_test 0x1234 1\n\n");
		return -1;
	}

	acc_name = argv[1];
	count = atoi(argv[2]);
	double bw;
	real_in_size = count * sysconf(_SC_PAGESIZE);
	real_out_size = count * sysconf(_SC_PAGESIZE);
	in_buffer_size = real_in_size;
	out_buffer_size = real_out_size;
	printf("in_buffer_size = %d\n", real_in_size);

	//opening accelerator
	if (count >= 1024){
		in_buffer_size = 1024 * sysconf(_SC_PAGESIZE);
		out_buffer_size = 1024 * sysconf(_SC_PAGESIZE);
	}

	//if a job is lareger than 4M, we allocate an 4M size acc to this job and copy data to acc_handler->data_buffer iterately.
	gettimeofday(&open_start, NULL);	
	in_buffer = pri_acc_open(&acc_handler, acc_name, in_buffer_size, out_buffer_size, section_id);
	gettimeofday(&open_end, NULL);
	timersub(&open_end, &open_start, &open_dt);
	long open_usec = open_dt.tv_usec + 1000000 * open_dt.tv_sec;
	printf("[OPEN] %s takes %ld microseconds.\n", acc_name, open_usec);

	if (in_buffer == NULL) {
		printf ("Open %s fail.\n", acc_name);
	} 
	else {
		unsigned int i, job_len;
		gettimeofday(&exe_start, NULL);
		for(i = 0; i<real_in_size; i+=in_buffer_size){
		    if ((real_in_size - i) >= in_buffer_size)
			job_len = in_buffer_size;
		    else 
			job_len = real_in_size - i;
		    if(job_len > 0){
			in_buffer = memset(in_buffer, 1, job_len);
		    	ret = acc_do_job(&acc_handler, param, job_len, &result_buffer);
		    	if (ret < 0)
				printf ("Do job fail. code = 0x%lx\n", ret);
		    }    
		}
		gettimeofday(&exe_end, NULL);
		timersub(&exe_end, &exe_start, &exe_dt);
		long job_usec = exe_dt.tv_usec + 1000000 * exe_dt.tv_sec;
		printf("[EXECUTION] %s takes %ld microseconds.\n", acc_name, job_usec);
		bw = ((double)(real_in_size))/(1024*1024)/((1.0*job_usec)/1000000.0);
		printf("[BW] is %f\n", bw);
	}
	// Here we close the accelerator
	acc_close(&acc_handler);

	return 0;
}
