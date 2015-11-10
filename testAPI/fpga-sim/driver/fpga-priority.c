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
	int fd, rc, set_max;
	unsigned int priority;
	struct ioctl_user_priority upriority;
	char *devnode = "/dev/fpgacom";

	if((argc != 4) && (argc != 5)){
		printf( "\033[47;30mUsage:\033[0m Command port_id pid priority [-M]\n" );
		exit(0);
	}


	upriority.port_id = atoi(argv[1]);
	upriority.user_pid = atoi(argv[2]);
	priority = atoi(argv[3]);

	if (priority > 1) {
		printf ("The priority should not be larger than 1.\n");
		return 0;
	}

	if (argc == 4)
		set_max = 0;
	else {
		if ((argv[4][0] == '-') && (argv [4][1] == 'M'))
			set_max = 1;
		else {
			printf ("Don't recogize parameter : %s\n", argv [4]);
			return 0;
		}
	}

	if (set_max)
		upriority.priority = (priority << 16) | 0xf0000000;
	else
		upriority.priority = priority;

	if(( fd = open(devnode, O_RDWR ))<0){
		printf( "Error:can not open the device: %s\n", devnode);
		return -1;
	}
	
	if(rc = ioctl(fd, IOCTL_USER_PRIORITY, &upriority)) {
		printf("ioctl error\n");
		return -1;
	}
	
	if(upriority.ret == -2)
		printf("\nThe acc(id=%d) does not exist.\n", upriority.port_id);
	else if(upriority.ret == -1)
		printf("\nThe user(pid=%d) does not exist.\n", upriority.user_pid);
	else if (upriority.ret == -3)
		printf("\nSet priority fail. The allowed highest priority is %d.\n", (upriority.priority>>16) & 0xff);
	else {
		if (set_max)
			printf("\n\033[47;30mSet Highest Priority : %d to PID : %d\033[0m\n\n", (priority>>16) & 0xff, upriority.user_pid);
		else
			printf("\n\033[47;30mSet Priority : %d to PID : %d\033[0m\n\n", priority & 0xff, upriority.user_pid);
//		printf("\033[47;30m  PID     ACC ID   PRIORITY\033[0m\n");
//		printf(" %d       %d        %d\n", upriority.user_pid, upriority.port_id, priority & 0xffff);
	}
	
	close(fd);
	return 0;
}
