# ifndef FPGACOM_H
# define FPGACOM_H


#include <linux/spinlock.h>
//#include <asm/system.h>
#include <linux/module.h>
#include "fpga-libacc.h"



struct acc_user_t {
	unsigned int user_pid;
	int count;
	unsigned int priority;
	unsigned long long limited_bandwith;	// Max bandwidth (MB/s) allow for this user.
	unsigned long long credit_left;			// how many data (Byte) can be issue in current second
	unsigned long long time_us;
	unsigned long long history_payload;		// for bandwidth computation
	unsigned long long history_time;
	struct acc_user_t *next;
};


struct acc_load_t {
	int port_id;							// the port that the acc has been configured at
	unsigned long long limit_bw;
	struct acc_info_t * acc_info;
	struct acc_user_t * acc_user;
	struct acc_load_t * next;
	spinlock_t acc_lock;
};


struct acc_port_status_t {
	struct acc_load_t * acc_load;		// What ACC has been opened in this port
	unsigned long long acc_id;			// What ACC has been reconfigured to this port
};

struct fpga_load_t {
	char used_port [MAX_ACC_PORT_NUM];   // '1' means this port is in used
	struct acc_port_status_t port_status [MAX_ACC_PORT_NUM];
	int rsc;							// how many resource are left
	struct fpga_info_t fpga_info;
	struct acc_load_t * acc_load;
	struct fpga_load_t * next;
	spinlock_t fpga_lock;
};


struct wukong_t {
	struct fpga_load_t * fpga_load;
	spinlock_t wukong_lock;
};


struct fpga_list_entry_t {
	struct fpga_info_t fpga_info;
	struct fpga_list_entry_t * next;
};


struct fpgacom_private_t {
	struct fpga_load_t * fpga_load;
	struct acc_load_t * acc_load;
	struct acc_user_t * acc_user;
	struct fpgacom_private_t * next;
};

struct acc_job_t {
	struct acc_descriptor des;
	unsigned long long acc_fd;
};

# endif
