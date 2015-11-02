#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <linux/ioctl.h>
#include "./fpga-libacc.h"

int main(int argc, char *argv[]) {
	int fd, rc;
	int bandwidth;
	struct ioctl_user_bandwidth ubw;
	char *devnode = "/dev/fpgacom";

	if( argc<4 ){
		printf( "\033[47;30m[Usage:\033[0m bandwidth port_id pid bandwidth(MB/s)]\n" );
		exit(0);
	}


	ubw.port_id = atoi(argv[1]);
	ubw.user_pid = atoi(argv[2]);
	bandwidth = atoi(argv[3]);

	if (bandwidth < 0)
		ubw.bandwidth = ~0UL;
	else
		ubw.bandwidth = (unsigned long)bandwidth;

	if(( fd = open(devnode, O_RDWR ))<0){
		printf( "Error:can not open the device: %s\n", devnode);
		return -1;
	}
	
	if(rc = ioctl(fd, IOCTL_USER_BANDWIDTH, &ubw)) {
		printf("ioctl error\n");
		return -1;
	}
	
	if(ubw.ret == -2)
		printf("The acc(id=%d) does not exist.\n", ubw.port_id);
	else if(ubw.ret == -1)
		printf("The user(pid=%d) does not exist.\n", ubw.user_pid);
	else {
		printf("\033[47;30m  PID     ACC ID   BANDWIDTH\033[0m\n");
		printf(" %d       %d      %lld MB/s\n", ubw.user_pid, ubw.port_id, ubw.bandwidth);
	}
	
	close(fd);
	return 0;
}
