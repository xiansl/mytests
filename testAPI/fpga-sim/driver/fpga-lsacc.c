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
	int fd, rc, opt, i;
	char *devnode = "/dev/fpgacom";
	char raw_data [512];
	struct acc_list_entry_t * acc_list;
	int bandwidth = 0, max_bandwidth;
	int rsc = 0;
	int detail = 0;
	
    while ((opt = getopt(argc, argv, "d")) != -1)
    {
        switch(opt)
        {
          case 'd':
			  detail = 1;
			  break;
		  default:
			  break;
		}
	}

	if(( fd = open(devnode, O_RDWR ))<0){
		printf( "Error:can not open the device: %s\n", devnode);
		return -1;
	}

	if (detail)
		printf("\033[47m\n\033[47;30m           ACC NAME (ID)            MAX-BANDWIDTH   RESOURCES  PORT  PATH\033[0m\n");
	else
		printf("\033[47m\n\033[47;30m           ACC NAME (ID)            MAX-BANDWIDTH   RESOURCES\033[0m\n");

	raw_data [0] = 1;
	acc_list = (struct acc_list_entry_t *) (&raw_data [64]);
	while (raw_data [0]) {
		if(rc = ioctl(fd, IOCTL_FPGACOM_LSACC, raw_data)) {
			printf("Can not get accelerator list in /dev/fpgacom.\n");
			return -1;
		}
		if (raw_data [0])
			if (detail) {
				for (i = 0; i < MAX_ACC_PORT_NUM; i ++) {
					if (acc_list->port_map[i])
						printf("  %10s (%016llx)    %8.2f MB/s       %2d%%      %d   %s\n", acc_list->acc_info.name, acc_list->acc_info.acc_id, (double)acc_list->acc_info.max_bw, acc_list->acc_info.rsc, i, acc_list->path);
				}
			} else
				printf("  %10s (%016llx)    %8.2f MB/s       %d%%\n", acc_list->acc_info.name, acc_list->acc_info.acc_id, (double)acc_list->acc_info.max_bw, acc_list->acc_info.rsc);
	}

	close(fd);
	return 0;
}
