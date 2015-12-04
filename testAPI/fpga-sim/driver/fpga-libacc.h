
//#define GUEST
//#define VM_BLOCK_MEM

# ifndef FPGA_LIBACC_H
# define FPGA_LIBACC_H

// The WUKONG_PAGE_SHIFT define the memory page alignment requirement
// It is a "at least" requirment
#define WUKONG_PAGE_SHIFT	12
#define WUKONG_REG_SIZE		4096

#define MAX_DES_NUM			8
#define ACC_NAME_SIZE		32
#define MAX_ACC_PORT_NUM	16
#define IOC_MAX_SIZE		512
#define MAX_PATH_SIZE		256


// Register defination in PFGA

#define ACC_DESCRIPTOR_OFFSET	0U
#define RECONFIG_OFFSET			8U
#define DMA_RESULT_ADDR_OFFSET	16U
#define ENDIAN_TEST_OFFSET		24U
#define FPGA_RESET_OFFSET		32U
#define REGISTER_TEST_OFFSET	40U

#define VIRTIO_OFFSET			(1*1024U)
#define GUEST_TO_HOST_PAGE_L	(VIRTIO_OFFSET + 0U)
#define GUEST_TO_HOST_PAGE_H	(VIRTIO_OFFSET + 4U)
#define GUEST_FROM_HOST_PAGE_L	(VIRTIO_OFFSET + 8U)
#define GUEST_FROM_HOST_PAGE_H	(VIRTIO_OFFSET + 12U)
#define GUEST_ACC_LS_OFFSET		(VIRTIO_OFFSET + 16U)
#define GUEST_ACC_ADD_OFFSET	(VIRTIO_OFFSET + 20U)
#define GUEST_ACC_RM_OFFSET		(VIRTIO_OFFSET + 24U)
#define GUEST_ACC_MALLOC_OFFSET	(VIRTIO_OFFSET + 28U)
#define GUEST_ACC_FREE_OFFSET	(VIRTIO_OFFSET + 32U)
#define GUEST_ACC_WDES_OFFSET	(VIRTIO_OFFSET + 36U)
#define GUEST_PHA_L_OFFSET		(VIRTIO_OFFSET + 40U)
#define GUEST_PHA_H_OFFSET		(VIRTIO_OFFSET + 44U)

struct acc_info_t {
	char name[ACC_NAME_SIZE];
	unsigned int type;
	unsigned int rsc;					//how many percentage of resource
	unsigned long long acc_id;
	unsigned long long vendor_id;
	unsigned long long max_bw;			//Max bandwidth (MB/s) of this acc.
};

struct acc_list_entry_t {
	char port_map [MAX_ACC_PORT_NUM];	// '1' means acc can be reconfigured to this port
	struct acc_info_t acc_info;
	char path [MAX_PATH_SIZE];
	struct acc_list_entry_t * next;
};

struct fpga_info_t {
	char port_map [MAX_ACC_PORT_NUM];	// '1' means fpga has this port
	char vendor_name [64];
	char fpga_name [64];
	char platform_name [64];
	int platform_id;
};

struct fpga_ls_fpga_t {
	char vendor_name [64];
	char fpga_name [64];
	char platform_name [64];
	int platform_id;
	int rsc;
};

struct fpga_ls_acc_t {
	char name [ACC_NAME_SIZE];
	char vendor [64];
	int port_id;
	int rsc;
	unsigned long long acc_id;
	unsigned long long max_bw;
	unsigned long long limit_bw;
};

struct fpga_top_acc_t {
	char name [ACC_NAME_SIZE];
	int port_id;
	unsigned long long acc_id;
	unsigned long long max_bw;
	unsigned long long limit_bw;
};

struct fpga_top_user_t {
	int priority;
	int pid;
	unsigned long long history_payload;
	unsigned long long history_time;
	unsigned long long limit_bw;
};


#define IOCTL_FPGACOM_LS	_IOWR('F', 0, char [IOC_MAX_SIZE])
#define IOCTL_FPGACOM_TOP	_IOWR('F', 1, char [IOC_MAX_SIZE])


#define IOCTL_ACC_WDES		_IOWR('F', 2, struct ioctl_acc_wdes)
struct ioctl_acc_wdes {
	unsigned long long key;
	unsigned long long ret;
};


#define IOCTL_ADD_ACC		_IOWR('F', 3, struct ioctl_add_acc)
struct ioctl_add_acc {
	char acc_name[ACC_NAME_SIZE];
	unsigned int acc_id;
	int exist;
};


#define IOCTL_RM_ACC		_IOWR('F', 4, struct ioctl_rm_acc)
struct ioctl_rm_acc {
	char acc_name[ACC_NAME_SIZE];
	unsigned int acc_id;
	int exist;
};


#define IOCTL_ACC_OPEN		_IOWR('F', 5, struct ioctl_acc_open)
#define IOCTL_ACC_CLOSE		_IOW('F', 6, unsigned long long)
struct ioctl_acc_open {
	char name [ACC_NAME_SIZE];
	unsigned long long acc_id;
	unsigned long long acc_fd;
	int by_acc_id;
	int port_id;
	int error_flag;
};

#define IOCTL_VM_ALLOC		_IOR('F', 7, struct ioctl_vm_mem_addr)
struct ioctl_vm_mem_addr {
	unsigned long vm_hpa;			// host physical address
	unsigned long vm_hka;			// host kernel address
	unsigned long vm_size;
};

#define IOCTL_USER_BANDWIDTH	_IOWR('F', 9, struct ioctl_user_bandwidth)
struct ioctl_user_bandwidth {
	unsigned int port_id;
	unsigned int user_pid;
	int ret;
	int padding;
	unsigned long long bandwidth;
};

#define IOCTL_FPGACOM_LSACC	_IOWR('F', 10, char [IOC_MAX_SIZE])

#define IOCTL_ACC_MALLOC	_IOWR('F', 12, struct ioctl_acc_malloc)
struct ioctl_acc_malloc {
	unsigned int data_size;
	unsigned int result_size;
	unsigned long long acc_fd;
	unsigned long long key;
};


#define IOCTL_ACC_FREE		_IOW('F', 13, unsigned long long)

#define IOCTL_USER_PRIORITY	_IOWR('F', 14, struct ioctl_user_priority)
struct ioctl_user_priority {
	unsigned int port_id;
	unsigned int user_pid;
	unsigned int priority;
	int ret;
};

#define IOCTL_ACC_LIST	_IOWR('F', 15, struct ioctl_acc_list)
struct ioctl_acc_list {
	char name[ACC_NAME_SIZE];
	unsigned int port;
	unsigned int bandwidth;
	unsigned long long acc_id;
	char path [MAX_PATH_SIZE];
};


struct acc_descriptor {
	unsigned int priority;
	unsigned int port_id;
	unsigned int data_len;
	unsigned int result_len;
	unsigned long long data_addr;         //8B
	unsigned long long result_addr;
	unsigned int key[4];
	unsigned long long des_addr;
	unsigned long long status;
};


/****************struct************/


struct acc_req {
	char acc_name[ACC_NAME_SIZE];
};

struct acc_handler_t {
	char acc_name[ACC_NAME_SIZE];
	unsigned long long acc_fd;
	int fpgacomFp;
	int fpgaFp;
	unsigned long time_out_counter;
	unsigned int data_buffer_size;
	unsigned int result_buffer_size;
	unsigned int * cmd_page;
	void * data_buffer;
	void * result_buffer;
	struct acc_descriptor * des;
	unsigned long long des_key;
};


struct server_param_t { 
    char job_id[16];
	char ipaddr[16];
	char port[16];
	char section_id[16];
	char status[16];
	char acc_name[16]; 
    char scheduler_host[16];
    char scheduler_port[16];
    int pipe[2];
    unsigned int in_buf_size;
    unsigned int out_buf_size;
};

int __acc_open(struct acc_handler_t * acc_handler, int port_id);
void __acc_close(struct acc_handler_t * acc_handler);


/* Return
 *     -1 : Time out error
 *     1  : Job is reset
 *     > 1: Result length = (RETURN & 0xfffffffeUL)
 */
long __acc_do_job (struct acc_handler_t *acc_handler);


void __job_init (struct acc_handler_t * acc_handler, const char * acc_name, unsigned int input_buf_pages, unsigned int output_buf_pages);

static inline void __job_set_param (struct acc_handler_t * acc_handler, const char * param)
{
	memcpy (acc_handler->des->key, param, 16);
}

static inline void __job_set_data_len (struct acc_handler_t * acc_handler, unsigned int len)
{
	acc_handler->des->data_len = len;
}

static inline void __job_set_time_out (struct acc_handler_t * acc_handler, unsigned long counter)
{
	acc_handler->time_out_counter = counter;
}


/* Return :
 *		NULL, if open the accelerator fail.
 *		non NULL, if open the accelerator ok.
 * acc_handler : the accelerator handler
 * acc_name : the name of the accelerator
 * in_buf_size : the size of the input buffer (bytes)
 * out_buf_size : the size of the output buffer (bytes)
 * port_id : -1 means do not specify the port
 */
void * pri_acc_open(struct acc_handler_t * acc_handler, const char * acc_name, unsigned int in_buf_size, unsigned int out_buf_size, int port_id);




/* Return :
 *		NULL, if open the accelerator fail.
 *		non NULL, if open the accelerator ok.
 * acc_handler : the accelerator handler
 * acc_name : the name of the accelerator
 * in_buf_size : the size of the input buffer (bytes)
 * out_buf_size : the size of the output buffer (bytes)
 */
void * acc_open(struct acc_handler_t * acc_handler, const char * acc_name, unsigned int in_buf_size, unsigned int out_buf_size);



/* Return
 *     -1 : Time out error
 *     1  : Job is reset
 *     > 1: Result length = (RETURN & 0xfffffffeUL)
 */
long acc_do_job (struct acc_handler_t *acc_handler, const char * param, unsigned int job_len, void ** result_buf);


void acc_close(struct acc_handler_t * acc_handler);

# endif
