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
	int fd, rc, i, j;
	char raw_data [512];
	char *devnode = "/dev/fpgacom";
	unsigned int bandwidth, max_bandwidth;
	int rsc = 0;
	struct fpga_ls_fpga_t * fpga_ls_fpga;
	struct fpga_ls_acc_t * fpga_ls_acc;

	if(( fd = open(devnode, O_RDWR ))<0){
		printf( "Error:can not open the device: %s\n", devnode);
		return -1;
	}


	fpga_ls_fpga = (struct fpga_ls_fpga_t *) &raw_data [64];
	fpga_ls_acc = (struct fpga_ls_acc_t *) &raw_data [64];
	
	raw_data [0] = 1;
	do {
		raw_data [1] = 0;
		if(rc = ioctl(fd, IOCTL_FPGACOM_LS, raw_data)) {
			printf("Can not read data from /dev/fpgacom\n");
			close(fd);
			return -1;
		}

		if (raw_data [0]) {
			printf("\033[47m\n\033[47;30m    FPGA VENDOR         FPGA NAME             PLATFORM NAME    PLATFORM ID    RESOURCES\033[0m\n");
			printf("    %-16s    %-18s    %-16s    %-8X    %d%%\n", fpga_ls_fpga->vendor_name, fpga_ls_fpga->fpga_name, fpga_ls_fpga->platform_name, fpga_ls_fpga->platform_id, (100 - fpga_ls_fpga->rsc));

			raw_data [1] = 1;
			printf("\033[47m\n    \033[47;30mACC NAME  PORT ID   VENDOR      MAX-BANDWIDTH  LIMITED-BANDWIDTH  RESOURCES\033[0m\n");
			do {
				if(rc = ioctl(fd, IOCTL_FPGACOM_LS, raw_data)) {
					printf("Can not read data from /dev/fpgacom\n");
					close(fd);
					return -1;
				}

				if (raw_data [1]) {
					printf("    %-10s  %3d     %-8s     %6lld MB/s    %8lld MB/s    %4d%%\n", fpga_ls_acc->name, fpga_ls_acc->port_id, fpga_ls_acc->vendor, fpga_ls_acc->max_bw, fpga_ls_acc->limit_bw, fpga_ls_acc->rsc);
				}
			} while (raw_data [1]);
		}
	} while (raw_data [0]);

	close(fd);
	return 0;
}
