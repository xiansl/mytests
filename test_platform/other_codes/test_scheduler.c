#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include "../acc_servicelayer.h"


int main(int argc, char **argv)
{	
	long ret;
	char *acc_name;
	struct timeval start, end, dt;
	//char * ptr;
	void * in_buffer[5];
	unsigned int in_buffer_size, out_buffer_size, real_in_size, real_out_size;
	char param[16];
	int count;
	param[0] = 0x24;

	if (argc != 3) {
		printf("Usage : ./test_bench <acc_name> <in_buffer_size>\n");
		return -1;
   	}
	struct acc_context_t my_context[5];

	acc_name = argv[1];
	count = atoi(argv[2]);
	real_in_size = count * sysconf(_SC_PAGESIZE);
	real_out_size = count * sysconf(_SC_PAGESIZE);

	printf("Request to open FPGA device:\n");
	in_buffer_size = real_in_size;
	out_buffer_size = real_out_size;
	if (count > 1024){
	in_buffer_size = 1024 * sysconf(_SC_PAGESIZE);
	out_buffer_size = 1024 * sysconf(_SC_PAGESIZE);
	}

	gettimeofday(&start, NULL);
	int i;
	for (i = 0; i<1; i++){
    	in_buffer[i] = fpga_acc_open(&(my_context[i]), acc_name, in_buffer_size, out_buffer_size);
	gettimeofday(&end, NULL);
	timersub(&end, &start, &dt);
	long open_usec = dt.tv_usec + 1000000 * dt.tv_sec;

	printf("[OPEN] %s takes %ld microseconds.\n", acc_name, open_usec);
	}

	if (in_buffer == NULL) {
		printf ("Open %s fail.\n", acc_name);
	}
	sleep(5);

	for (i = 0; i<1; i++){
    		fpga_acc_close(&my_context[i]);
	}

	return 0;
}

