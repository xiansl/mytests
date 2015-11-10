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
	int fd, rc, i, j, k, t;
	int acc_user_print;
	struct fpga_top_acc_t * fpga_top_acc;
	struct fpga_top_user_t * fpga_top_user;
	char raw_data [512];
	
	char *devnode = "/dev/fpgacom";
	unsigned long acc_bandwidth;
	double max_bandwidth, bandwidth;
	char flag [2];
	unsigned int priority;
	
	if(( fd = open(devnode, O_RDWR ))<0){
		printf( "Error:can not open the device: %s\n", devnode);
		return -1;
	}

	fpga_top_acc = (struct fpga_top_acc_t *) &raw_data [64];
	fpga_top_user = (struct fpga_top_user_t *) &raw_data [64];
	flag [1] = '\0';

	while (1) {
		k=0;
		printf("\033[47m\n\033[47;30m            ACC NAME (ID)        PORT   STATUS       \033[0m\n");
		k++;
		k++;

		raw_data [0] = 1;
		do {
			raw_data [1] = 0;
			if(rc = ioctl(fd, IOCTL_FPGACOM_TOP, raw_data)) {
				printf("Can not read data from %s\n", devnode);
				close (fd);
				return -1;
			}

			if (raw_data [0]) {
				if (fpga_top_acc->limit_bw > fpga_top_acc->max_bw)
					bandwidth = 100.0;
				else
					bandwidth = (double)(fpga_top_acc->limit_bw*100) / (double)fpga_top_acc->max_bw;

				printf ("\033[0m \033[0m\n");
				k++;

				printf("\033[0m  %10s (%016llx)    %-3d  %.2f%%\033[0m\n", fpga_top_acc->name, fpga_top_acc->acc_id, fpga_top_acc->port_id, bandwidth);
				k++;
				
				raw_data [1] = 1;
				acc_user_print = 1;
				do {
					if(rc = ioctl(fd, IOCTL_FPGACOM_TOP, raw_data)) {
						printf("Can not read data from %s\n", devnode);
						close (fd);
						return -1;
					}
					
					if (raw_data [1]) {
						if (acc_user_print) {
							printf("\033[43;30m            P   PID     BANDWIDTH       MAX BANDWIDTH\033[0m\n");
							k++;
							acc_user_print = 0;
						}

						if (fpga_top_user->priority & 0xf0000000) {
							priority = (fpga_top_user->priority>>16) & 0xff;
							flag [0] = '+';
						} else {
							priority = fpga_top_user->priority & 0xff;
							flag [0] = ' ';
						}
						bandwidth = ((double)fpga_top_user->history_payload)/((double) fpga_top_user->history_time) * (1000000.0/(1024.0*1024.0));
						printf("           %s%d   %-6d  %6.2f MB/s   %6lld MB/s\n", flag, priority,  fpga_top_user->pid, bandwidth, fpga_top_user->limit_bw);
						k++;
					}
				} while (raw_data [1]);
			}
		} while (raw_data [0]);

		usleep(400000);
		for(j=k; j>0; j--) {
			printf("\033[1A\033[K");
		}
	}

	close (fd);
	return 0;
}
