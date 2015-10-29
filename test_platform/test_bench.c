#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include "acc_servicelayer.h"

int main(int argc, char **argv)
{	
	long ret;
	char *acc_name;
	struct timeval start, end, dt;
	//char * ptr;
	void * in_buffer;
    	void *result_buf;
	unsigned int in_buffer_size, out_buffer_size, real_in_size, real_out_size;
	char param[16];
	int count;
	param[0] = 0x24;

	if (argc != 3) {
		printf("Usage : ./test_bench <acc_name> <workload>\n");
		printf("\tworkload must be a mutiple of 4 K bytes.\n");
		printf("\tExample : ./test_bench AES 1 (means executing AES with workloads of 1 * 4K bytes.\n");
		return -1;
   	}
	struct acc_context_t my_context;

	acc_name = argv[1];
	count = atoi(argv[2]);
	real_in_size = count * sysconf(_SC_PAGESIZE);
	real_out_size = count * sysconf(_SC_PAGESIZE);
	in_buffer_size = real_in_size;
	out_buffer_size = real_out_size;
	
	printf("job_size = %d K bytes, result_size=%d K bytes.\n", count*4, count*4);

	printf("Request to open FPGA device:\n");
	if (count >= 1024){
		in_buffer_size = 1024 * sysconf(_SC_PAGESIZE);
		out_buffer_size = 1024 * sysconf(_SC_PAGESIZE);
	}
	gettimeofday(&start, NULL);
    	in_buffer = fpga_acc_open(&(my_context), acc_name, in_buffer_size, out_buffer_size, real_in_size);
	gettimeofday(&end, NULL);
	timersub(&end, &start, &dt);
	long open_usec = dt.tv_usec + 1000000 * dt.tv_sec;

	printf("[OPEN] %s takes %ld microseconds.\n", acc_name, open_usec);

	if (in_buffer == NULL) {
		printf ("Open %s fail.\n", acc_name);
	}
	else {
		unsigned int i, job_len;
		gettimeofday(&start, NULL);
		for(i = 0; i<real_in_size; i+=in_buffer_size){
		    if ((real_in_size - i) >= in_buffer_size)
			job_len = in_buffer_size;
		    else 
			job_len = real_in_size - i;
		    in_buffer = memset(in_buffer, 1, job_len);
   		    ret = fpga_acc_do_job(&my_context, param, job_len, &result_buf);
		    if(ret < 0)
			printf ("do job fail. code = 0x%lx\n", ret);

	        }
		gettimeofday(&end, NULL);
		timersub(&end, &start, &dt);
		long job_usec = dt.tv_usec + 1000000 * dt.tv_sec;

		printf("[EXECUTION] %s takes %ld microseconds.\n", acc_name, job_usec);
	}

    gettimeofday(&start, NULL);
    fpga_acc_close(&my_context);
    gettimeofday(&end, NULL);
    timersub(&end, &start, &dt);
    long close_usec = dt.tv_usec + 1000000 * dt.tv_sec;
    //acc_context->total_time += dur_usec;
		printf("[CLOSE] %s takes %ld microseconds.\n", acc_name, close_usec);
	return 0;
}

