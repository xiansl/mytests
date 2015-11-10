#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <linux/ioctl.h>
#include "fpga-libacc.h"
#include "reconfig.h"

int is_openstack_format (char * file_buffer)
{
	struct extra_header_t * header_ptr = (struct extra_header_t *) file_buffer;

	if (myntohll(header_ptr->header_id) == HEADER_ID)
		return 1;
	else
		return 0;
}

char * read_disk_file (char * file_name, unsigned int * fsize_ptr)
{
	FILE * fp;
	unsigned int fsize;
	char * file_buffer;
	unsigned int ret;

	fp = fopen(file_name, "r");
	if (fp == NULL) {
		printf ("Error:can not open file : %s\n", file_name);
		return NULL;
	}

	fseek (fp, 0, SEEK_END);
	fsize = ftell (fp);
	fseek (fp, 0, SEEK_SET);

	file_buffer = (char *) malloc (fsize);
	if (NULL == file_buffer) {
		printf ("Error:Can not read file %s. Out of memory.\n", file_name);
		return NULL;
	}

	ret = fread (file_buffer, fsize, 1, fp);
	fclose (fp);
	if (ret < 1) {
		printf ("Error occur when reading file %s from disk.\n", file_name);
		return NULL;
	}

	*fsize_ptr = fsize;
	return file_buffer;
}



void print_help (void)
{
	printf( "\nUsage: fpga-addacc [-pnbfh] ACC_NAME\n" );
	printf( "       -n : specify the ACC name\n" );
	printf( "       -i : specify the ACC ID (64bit value like 0x1234)\n" );
	printf( "       -p : specify the port number\n" );
//	printf( "       -n : specify the path of new accelerator file\n" );
	printf( "       -b : specify the bandwidth of new accelerator\n" );
	printf( "       -f : specify the OpenStack Format file (Full Path)\n" );
	printf( "       -h : print this help message\n" );
	exit (0);
}

int main(int argc, char **argv){
	int fd, rc, opt;
	struct ioctl_acc_list acc_new;
	char *devnode = "/dev/fpgacom";
	unsigned int port_id = (-1);
	unsigned int bandwidth = (-1);
	unsigned long long acc_id = 0;
	int add_by_file = 0;

	unsigned int fsize, index;
	char * file_buffer;
	struct extra_header_t * header_ptr;

	acc_new.path [0] = '\0';
	acc_new.name [0] = '\0';

	while ((opt = getopt(argc, argv, "p:n:b:i:f:h")) != -1)
	{
		switch(opt)
		{
			case 'p':
				port_id = atoi(optarg);
				break;

			case 'i':
				acc_id = strtoull(optarg, NULL, 0);
				break;

			case 'b':
				bandwidth = atoi(optarg);
				break;

			case 'n':
				strcpy(acc_new.name, optarg);
				break;

			case 'f':
				add_by_file = 1;
				strncpy(acc_new.path, optarg, MAX_PATH_SIZE);
				break;

			case 'h':
				print_help();
				break;

			default:
				print_help();
				break;
		}
	}

	if (add_by_file) {
		if (acc_new.path [0] != '/') {
			printf ("Error. The file path MUST be a linux full path begin with \"/\"\n");
			return -1;
		}

		file_buffer = read_disk_file (acc_new.path, &fsize);
		if (file_buffer == NULL) {
			printf ("Error. Can not find the file %s\n", acc_new.path);
			return -1;
		}

		if (!is_openstack_format (file_buffer)) {
			printf ("Error. %s is not an Openstack-format file.\n", acc_new.path);
			free (file_buffer);
			return -1;
		}

		index = 0;
		header_ptr = (struct extra_header_t *) file_buffer;

		if ((fd = open(devnode, O_RDWR)) < 0) {
			printf( "Error:can not open the device: %s\n", devnode);
			return -1;
		}

		printf("\033[47m\n\033[47;30m          ACC NAME (ID)            PORT    BANDWIDTH     VENDOR\033[0m\n");

		while (1) {
			acc_new.port = myntohl(header_ptr->port_id);
			acc_new.bandwidth = 3000;
			acc_new.acc_id = myntohll(header_ptr->acc_id);
			strcpy(acc_new.name, header_ptr->acc_name);

			if (rc = ioctl(fd, IOCTL_ACC_LIST, &acc_new)) {
				printf("Error. can not add the ACC.\n");
				close(fd);
				return -1;
			}
			printf("%10s (0x%016llx)     %-3d  %6d MB/s     IBM\n", acc_new.name, acc_new.acc_id, acc_new.port, acc_new.bandwidth);

			if (header_ptr->next_header == 0)
				break;

			index += myntohl(header_ptr->next_header);
			if ((index + sizeof(struct extra_header_t)) > fsize)
				break;

			header_ptr = (struct extra_header_t *) (file_buffer+index);
		}

		free (file_buffer);
		close(fd);
	} else {
		if (port_id == (-1)) {
			printf ("Error. Using -f to specify a file or -p to specify a port.\n");
			print_help();
			return -1;
		}

		if (bandwidth == (-1)) {
			printf ("Error. Using -f to specify a file or -b to specify a bandwidth.\n");
			print_help();
			return -1;
		}

		if (acc_new.name [0] == '\0') {
			printf ("Error. Using -f to specify a file or -n to specify a name.\n");
			print_help();
			return -1;
		}

		if (acc_id == 0) {
			printf ("Error. Using -f to specify a file or -i to specify the ACC ID.\n");
			print_help();
			return -1;
		}

		acc_new.port = port_id;
		acc_new.bandwidth = bandwidth;
		acc_new.acc_id = acc_id;

		if ((fd = open(devnode, O_RDWR)) < 0) {
			printf( "Error:can not open the device: %s\n", devnode);
			return -1;
		}

		if (rc = ioctl(fd, IOCTL_ACC_LIST, &acc_new)) {
			printf("Error. can not add the ACC.\n");
			close(fd);
			return -1;
		}

		printf("\033[47m\n\033[47;30m          ACC NAME (ID)            PORT    BANDWIDTH     VENDOR\033[0m\n");
		printf("%10s (0x%016llx)     %-3d  %6d MB/s     IBM\n", acc_new.name, acc_new.acc_id, acc_new.port, acc_new.bandwidth);
		close(fd);
	}
	
	return 0;
}
