#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <linux/ioctl.h>
#include "./fpga-libacc.h"

int main(int argc, char **argv){
	int fd, rc, i;
	struct ioctl_rm_acc rm;
	char *devnode = "/dev/fpgacom";

	if( argc<2 ){
		printf( "[Usage: accrm accID]\n" );
		exit(0);
	}

	rm.acc_id = atoi(argv[1]);

	if(( fd = open(devnode, O_RDWR ))<0){
		printf( "Error:can not open the device: %s\n", devnode);
		return -1;
	}

	if(rc = ioctl(fd, IOCTL_RM_ACC, &rm)) {
		printf("ioctl error\n");
		return -1;
	}
	if(rm.exist == -1)
		printf("Error: the acc does not exist\n");
	else if(rm.exist == -2)
		printf("Error: the acc is in use\n");
	else
		printf("\033[47m\n\033[47;30m    ACC NAME      ACC ID    VENDOR\033[0m\n    %-10s    %-6d    IBM\n", rm.acc_name, rm.acc_id);

	close(fd);
	return 0;
}
