#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <memory.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/time.h>
#include "common.h"

#define BIT64_LOW	0x00000000ffffffff
#define BIT64_HIGH	0xffffffff00000000

int page_num = 1;


void print_usage (void)
{
	printf("Usage: test_pcie flag offset [value]\n");
	printf("\tflag:\n\t\t0 : read32\n\t\t1 : read64\n\t\t2 : write32\n\t\t3 : write64\n\t\t4 : read32 [value times]\n");
}

int main(int argc, char **argv) {
	unsigned long *virtaddr = NULL;
	unsigned long flag, offset, ivalue64;
	unsigned int ivalue32;
	int fd, i, sum;
	volatile unsigned int * ret32;
	volatile unsigned long * ret64;
	unsigned char * ptr;
	struct timeval start, end, dt;
	unsigned int value32;
	unsigned long value64;
	unsigned long long begin_cycle, end_cycle;

	if(argc == 3) {
		flag = strtoul(argv[1], NULL, 0);
		offset = strtoul (argv [2], NULL, 0);
	}
	else if(argc == 4) {
		flag  = strtoul(argv[1], NULL, 0);
		offset = strtoul (argv [2], NULL, 0);
		ivalue64 = strtoul (argv [3], NULL, 0);
		ivalue32 = ivalue64;
	} else {
		print_usage ();
		exit(0);
	}

	if ((flag >= 2) && (argc == 3)) {
		print_usage ();
		exit(0);
	}

	if((fd = open("/dev/fpgacom", O_RDWR)) < 0) {
		printf("Error: can not open the device: /dev/fpgacom\n");
		exit(0);
	}

	virtaddr = (unsigned long *)mmap(NULL, (page_num*sysconf(_SC_PAGESIZE)), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if((virtaddr == NULL) || (errno != 0)) {
		printf("mmap error, errno is %d\n", errno);
		close (fd);
		return 0;
	}

	switch (flag) {
		case 0 :
			ret32 = (unsigned int *) (((char *) virtaddr) + offset);
			value32 = *ret32;
			printf ("Read32 0x%lx = 0x%x.\n", offset, value32);
			break;
		case 1 :
			ret64 = (unsigned long *) (((char *) virtaddr) + offset);
			value64 = *ret64;
			printf ("Read64 0x%lx = 0x%lx.\n", offset, value64);
			break;
		case 2 :
			ret32 = (unsigned int *) (((char *) virtaddr) + offset);
			*ret32 = ivalue32;
			ptr = (unsigned char *) &ivalue32;
			printf ("Write32 0x%lx = 0x%x. [0]-[3]:%02x_%02x_%02x_%02x\n", offset, ivalue32, ptr [0], ptr [1], ptr [2], ptr [3]);
			break;
		case 3 :
			ret64 = (unsigned long *) (((char *) virtaddr) + offset);
			*ret64 = ivalue64;
			ptr = (unsigned char *) &ivalue64;
			printf ("Write64 0x%lx = 0x%lx. [0]-[7]:%02x_%02x_%02x_%02x_%02x_%02x_%02x_%02x\n", offset, ivalue64, ptr [0], ptr [1], ptr [2], ptr [3], ptr [4], ptr [5], ptr [6], ptr [7]);
			break;
		case 4 :
			ret32 = (unsigned int *) (((char *) virtaddr) + offset);
			gettimeofday(&start, NULL);
			for (i = 0; i < ivalue64; i ++) {
				sum += * ret32;
			}
			gettimeofday(&end, NULL);
			timersub(&end, &start, &dt);
			printf ("Loop %ld times. %ld us spent.\n", ivalue64, (dt.tv_usec + 1000000 * dt.tv_sec));
			break;
	}

	munmap(virtaddr, page_num*sysconf(_SC_PAGESIZE));

	close (fd);
	return 0;
}
