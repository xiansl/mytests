#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include <unistd.h>
#include "../driver/fpga-libacc.h"



//	Here is the sample that how software do a job with accelerator

long do_my_work(struct acc_handler_t * acc_handler, void * in_buffer) {

	long ret;
	unsigned char * ptr;
	char param [16];
	unsigned int job_len;
	void * result_buffer;

//	Here you set a software-accelerator co-defined parameter
	param [0] = 0x24;

//	Here you copy your source data into this buffer
//	Copy "your data" into "acc_handler->data_buffer"
	memset (in_buffer, 0, sysconf(_SC_PAGESIZE));

//	Here you set the real data length in the source data buffer
//	so that accelerator know the real data length in the buffer
	job_len = 1024;

//	Here the software sends the job to accelerator and wait completion
	ret = acc_do_job (acc_handler, param, job_len, &result_buffer);

	if (ret > 0) {
	//	Do your work with the result here

	//	Here you can read the result out of the result buffer
		ptr = (char *) result_buffer;

	//	Here is some sample code to print out the result.
		printf ("\nTest complete. Result :\n");
		printf ("\t%02x_%02x_%02x_%02x_%02x_%02x_%02x_%02x\n", ptr [0], ptr [1], ptr [2], ptr [3], ptr [4], ptr [5], ptr [6], ptr [7]);
		printf ("\t%02x_%02x_%02x_%02x_%02x_%02x_%02x_%02x\n\n", ptr [8], ptr [9], ptr [10], ptr [11], ptr [12], ptr [13], ptr [14], ptr [15]);
	}

	return ret; 
}


/*************************************************/

int main (int argc, char **argv) {

	long ret, iters;
	struct acc_handler_t acc_handler;
	char * acc_name;
	void * in_buffer;
	unsigned int in_buffer_size, out_buffer_size;

	ret = 0;

	if (argc != 2) {
		printf ("Usage : micro_test ACC_name/ACC_ID\n");
		printf ("\tExample : micro_test AES\n");
		printf ("\tExample : micro_test 0x1234\n\n");
		return -1;
	}

	acc_name = argv[1];
	in_buffer_size = sysconf(_SC_PAGESIZE);
	out_buffer_size = sysconf(_SC_PAGESIZE);

	//opening accelerator
	//To simplify the test, we malloc 1 memory page for both data and result
	in_buffer = acc_open(&acc_handler, acc_name, in_buffer_size, out_buffer_size);

	if (in_buffer == NULL) {
		printf ("Open %s fail.\n", acc_name);
	} else {
		ret = do_my_work (&acc_handler, in_buffer);
		if (ret < 0)
			printf ("Do job fail. code = 0x%lx\n", ret);
		else
			printf ("Job finishes. 0x%lx bytes of result are ready.\n", ret);
	}
	// Here we close the accelerator
	acc_close(&acc_handler);

	return 0;
}
