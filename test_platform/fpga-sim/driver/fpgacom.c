#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/kmod.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <asm/io.h>
#include <asm/ioctl.h>
#include <asm/uaccess.h>

#include "./fpgacom.h"
#include "./fpgadev.h"

MODULE_LICENSE("Dual BSD/GPL");

extern volatile int *ioremap_addr;
extern struct fpga_dev_t fpga_dev;

// A list of accelerators that can be reconfigured into the FPGA
struct acc_list_entry_t * acc_list;

struct wukong_t wukong;

// A global variable that keeps all info in the whole platform
// Three chains are used to keep those information
// wukong -> fpga_info -> acc_load -> acc_user
//               |           |           |
//           fpga_info    acc_load    acc_user


struct bw_control_t {
	unsigned int working;
	unsigned int len;
	unsigned long bandwidth;
	unsigned long ticket_span;
	unsigned long last_stamp;
	unsigned long port_credit_left;
	unsigned long port_last_sec;
	spinlock_t port_trylock;
	char pad [64-48-sizeof(spinlock_t)];
} __attribute__ ((aligned (64)));

spinlock_t host_trylock;
// host_span = (46K/1600M)*1000000 us
#define host_span 18UL
unsigned long host_stamp = 0;

struct bw_control_t bw_control_array [4];
unsigned int bw_control_priority = 0;


static int fpgacom_major = 0;		/* major device number */
static int fpgacom_minor = 0;

static struct cdev fpgacomDev;
struct class *fpgacom_class = NULL;

static int chmod_node(int if_com) {
	char path[] = "/dev/fpgacom";
	char *argv[] = { "/bin/chmod", "0666", path, NULL};
	static char *envp[] = {
		"HOME=/",
		"TERM=linux",
		"PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL};
	if (! if_com)
		path[9] = '\0';

	return call_usermodehelper( argv[0], argv, envp, UMH_WAIT_PROC);
}


static int reconfig_fpga (char * file_path, char * port_id) {
	printk ("/usr/bin/fpga-reconfig -p %s %s\n", port_id, file_path);
	return 0;
}


void add_to_acc_list (char * name, char * path, unsigned long long acc_id, unsigned int port, unsigned long bandwidth) {
	int find_acc;
	struct acc_list_entry_t * next_ptr, * ptr, *new_acc = NULL;

	find_acc = 0;
	ptr = acc_list;
	next_ptr = acc_list;
	while (next_ptr != NULL) {
		if (acc_id == next_ptr->acc_info.acc_id) {
			next_ptr->port_map [port] = 1;
			find_acc = 1;
			break;
		}
		ptr = next_ptr;
		next_ptr = next_ptr->next;
	}

	if (find_acc == 0) {
		new_acc = kmalloc (sizeof(struct acc_list_entry_t), GFP_KERNEL);
		memset (new_acc->port_map, 0, MAX_ACC_PORT_NUM);
		new_acc->port_map [port] = 1;
		strncpy (new_acc->acc_info.name, name, ACC_NAME_SIZE);
		if (path != NULL)
			strncpy (new_acc->path, path, MAX_PATH_SIZE);
		else
			new_acc->path [0] = '\0';
		new_acc->acc_info.acc_id = acc_id;
		new_acc->acc_info.rsc = 15;
		new_acc->acc_info.max_bw = bandwidth;
		new_acc->next = NULL;

		if (next_ptr == acc_list)
			acc_list = new_acc;
		else
			ptr->next = new_acc;
	}
};


int delete_from_acc_list (unsigned long long acc_id) {

	struct acc_list_entry_t * next_ptr, * ptr;

	ptr = acc_list;
	next_ptr = acc_list;
	while (next_ptr != NULL) {
		if (acc_id == next_ptr->acc_info.acc_id) {
			if (next_ptr == acc_list)
				acc_list = next_ptr->next;
			else
				ptr->next = next_ptr->next;

			kfree(next_ptr);
			return 0;
		}
		ptr = next_ptr;
		next_ptr = ptr->next;
	}

	return -1;
};


void create_acc_list (void) {
	struct file *filp;
	struct inode * inode;
	mm_segment_t fs;
	off_t fsize;
	char name [ACC_NAME_SIZE];
	unsigned long bandwidth, hard_id;
	unsigned long long acc_id;
	char * head, * tail, * buf = NULL;

	filp=filp_open ("/home/900/lxs900/zzd/test_platform/fpga-sim/driver/wukong.conf", O_RDONLY, 0);
	if (IS_ERR (filp)) {
		printk ("Warning: /home/900/lxs900/zzd/test_platform/fpga-sim/driver/wukong.conf is not found. No ACC will be enable.\n");
		return;
	}

	inode = filp->f_dentry->d_inode;
	fsize=inode->i_size;
	buf = (char *) kmalloc(fsize+1,GFP_ATOMIC);

	fs=get_fs();
	set_fs(KERNEL_DS);
//	vfs_read (filp, buf, fsize+1, 0);
	filp->f_op->read(filp,buf,fsize,&(filp->f_pos));
	set_fs(fs);

	filp_close (filp, NULL);

	buf [fsize] = '\0';
	head = buf;

	while (1) {
		if (head [0] == '#') {
			tail = strchr (head, '\n');
			if (tail == NULL)
				break;
			head = tail + 1;
			continue;
		}

		tail = strchr (head, ',');
		if (tail == NULL)
			break;
		tail[0] = '\0';
		if (strict_strtoul (head, 0, &hard_id))
			break;

		head = tail + 1;
		tail = strchr (head, ',');
		if (tail == NULL)
			break;
		tail[0] = '\0';
		if (strict_strtoull (head, 0, &acc_id))
			break;

		head = tail + 1;
		if (!(((head [0] >= 'A') && (head [0] <= 'Z')) || ((head [0] >= 'a') && (head [0] <= 'z'))))
			break;
		tail = strchr (head, ',');
		if (tail == NULL)
			break;
		tail [0] = '\0';
		strncpy (name, head, ACC_NAME_SIZE);

		head = tail + 1;
		tail = strchr (head, '\n');
		if (tail == NULL)
			break;
		tail [0] = '\0';
		if (strict_strtoul(head, 0, &bandwidth))
			break;

		head = tail + 1;

		if (hard_id < MAX_ACC_PORT_NUM) {
			add_to_acc_list (name, NULL, acc_id, hard_id, bandwidth);
			printk ("%s (0x%08llx) is supported by system at fpga port : %ld, max-bandwidth is %ld MB/s.\n", name, acc_id, hard_id, bandwidth);
		} else
			printk ("%s (0x%08llx) is NOT supported by system at fpga port : %ld. Exceed the MAX_ACC_PORT_NUM : %d\n", name, acc_id, hard_id, MAX_ACC_PORT_NUM);
	}

	kfree (buf);
}


ssize_t fpgacom_update_acc_list(unsigned char __user *arg) {

	struct ioctl_acc_list new_acc;

	if(copy_from_user(&new_acc, arg, sizeof(struct ioctl_acc_list)))
		return -EFAULT;

	add_to_acc_list (new_acc.name, new_acc.path, new_acc.acc_id, new_acc.port, new_acc.bandwidth);

	return 0;
}

ssize_t fpgacom_ioctl_lsacc(unsigned char __user *arg) {
	int rc = 0, i;
	struct acc_list_entry_t * acc_list_ptr;
	unsigned char raw_data [IOC_MAX_SIZE];

	if(copy_from_user(raw_data, arg, IOC_MAX_SIZE))
		return -EFAULT;

	acc_list_ptr = acc_list;
	for (i = 1; i < raw_data [0]; i ++) {
		if (acc_list_ptr != NULL)
			acc_list_ptr = acc_list_ptr->next;
		else 
			break;
	}

	if (acc_list_ptr != NULL) {
		memcpy (&raw_data [64], acc_list_ptr, sizeof (struct acc_list_entry_t));
		raw_data [0] ++;
	} else
		raw_data [0] = 0;

	if(copy_to_user(arg, raw_data, IOC_MAX_SIZE))
		rc  = -EFAULT;

	return rc;
}


ssize_t fpgacom_ioctl_ls (char __user *arg) {
	int rc = 0, i, j;
	struct fpga_load_t * fpgap;
	struct acc_load_t * accp;
	unsigned char raw_data [IOC_MAX_SIZE];
	struct fpga_ls_fpga_t * fpga_ls_fpga;
	struct fpga_ls_acc_t * fpga_ls_acc;

	if(copy_from_user(raw_data, arg, IOC_MAX_SIZE))
		return -EFAULT;

	fpgap = wukong.fpga_load;
	for (i = 1; i < raw_data [0]; i ++) {
		if (fpgap != NULL)
			fpgap = fpgap->next;
		else
			break;
	}

	if (fpgap == NULL)
		raw_data [0] = 0;
	else if (raw_data [1] == 0) {		// user is reading the FPGA information
		fpga_ls_fpga = (struct fpga_ls_fpga_t *) &raw_data [64];
		memcpy (fpga_ls_fpga->vendor_name, fpgap->fpga_info.vendor_name, 64);
		memcpy (fpga_ls_fpga->fpga_name, fpgap->fpga_info.fpga_name, 64);
		memcpy (fpga_ls_fpga->platform_name, fpgap->fpga_info.platform_name, 64);
		fpga_ls_fpga->platform_id = fpgap->fpga_info.platform_id;
		fpga_ls_fpga->rsc = fpgap->rsc;
	} else {							// user is reading the accelerator information
		accp = fpgap->acc_load;
		for (j = 1; j < raw_data [1]; j ++) {
			if (accp != NULL)
				accp = accp->next;
			else
				break;
		}

		if (accp == NULL) {
			raw_data [0] ++;
			raw_data [1] = 0;
		} else {
			fpga_ls_acc = (struct fpga_ls_acc_t *) &raw_data [64];
			memcpy (fpga_ls_acc->name, accp->acc_info->name, ACC_NAME_SIZE);
			strcpy (fpga_ls_acc->vendor, "IBM");
			fpga_ls_acc->port_id = accp->port_id;
			fpga_ls_acc->rsc = accp->acc_info->rsc;
			fpga_ls_acc->max_bw = accp->acc_info->max_bw;
			fpga_ls_acc->limit_bw = accp->limit_bw;
			raw_data [1] ++;
		}
	}

	if(copy_to_user(arg, raw_data, IOC_MAX_SIZE))
		rc = -EFAULT;
	
	return rc;
}


ssize_t fpgacom_ioctl_top(char __user *arg) {
	int rc = 0, i;
	unsigned char raw_data [IOC_MAX_SIZE];
	struct acc_load_t * accp;
	struct acc_user_t * accu;
	struct fpga_top_acc_t * fpga_top_acc;
	struct fpga_top_user_t * fpga_top_user;

	if(copy_from_user(raw_data, arg, IOC_MAX_SIZE))
		return -EFAULT;
	
	if (NULL == wukong.fpga_load)
		accp = NULL;
	else {
		accp = wukong.fpga_load->acc_load;
		for (i = 1; i < raw_data [0]; i ++) {
			if (accp != NULL)
				accp = accp->next;
			else
				break;
		}
	}
	
	if (accp == NULL)
		raw_data [0] = 0;
	else if (raw_data [1] == 0) {		// user is reading the ACC information
		fpga_top_acc = (struct fpga_top_acc_t *) &raw_data [64];
		memcpy (fpga_top_acc->name, accp->acc_info->name, ACC_NAME_SIZE);
		fpga_top_acc->port_id = accp->port_id;
		fpga_top_acc->acc_id = accp->acc_info->acc_id;
		fpga_top_acc->max_bw = accp->acc_info->max_bw;
		fpga_top_acc->limit_bw = accp->limit_bw;
	} else {							// user is reading the user information
		accu = accp->acc_user;
		for (i = 1; i < raw_data [1]; i ++) {
			if (accu != NULL)
				accu = accu->next;
			else
				break;
		}

		if (accu == NULL) {
			raw_data [0] ++;
			raw_data [1] = 0;
		} else {
			fpga_top_user = (struct fpga_top_user_t *) &raw_data [64];
			fpga_top_user->priority = accu->priority;
			fpga_top_user->pid = accu->user_pid;
			fpga_top_user->history_payload = accu->history_payload;
			fpga_top_user->history_time = accu->history_time;
			fpga_top_user->limit_bw = accu->limited_bandwith;
			raw_data [1] ++;
		}
	}

	if(copy_to_user(arg, raw_data, IOC_MAX_SIZE))
		rc = -EFAULT;

	return rc;

}


// The acc_id is unique while the acc_name is not.
// If "by_acc_id" is false, the function will return the first accelerator with
// the same name as "acc_name".
struct acc_load_t * load_acc (struct ioctl_acc_open * op) {
	int i, find_acc, in_list, acc_support;
	struct acc_load_t * accp, * acc_new, * acc_tail;
	struct acc_list_entry_t * acc_list_ptr;
	struct acc_load_t * rc = NULL;

	if (NULL == wukong.fpga_load) {
		op->error_flag = -1;
		return NULL;
	}

	// Check out whether this ACC has been loaded
	in_list = 0;
	accp = wukong.fpga_load->acc_load;
	acc_tail = accp;
	while (accp != NULL) {
		if (op->by_acc_id)
			find_acc = (accp->acc_info->acc_id == op->acc_id) ? 1 : 0;
		else
			find_acc = !strncmp(accp->acc_info->name, op->name, ACC_NAME_SIZE);
		if (find_acc) {
			// If not spedify the port ID, or the specified port ID exists
			// return existing port_id and do nothing.
			if ((op->port_id < 0) || (op->port_id == accp->port_id)) {
				rc = accp;
				in_list = 1;
				break;
			}
		}

		acc_tail = accp;
		accp = accp->next;
	}

	acc_support = 1;
	// If the ACC has not been loaded, check out whether the ACC is supported
	if (!in_list) {
												// Check out whether the ACC is supported
		acc_support = 0;
		acc_list_ptr = acc_list;
		while (acc_list_ptr != NULL) {
			if (op->by_acc_id)
				find_acc = (acc_list_ptr->acc_info.acc_id == op->acc_id) ? 1 : 0;
			else
				find_acc = !strncmp (acc_list_ptr->acc_info.name, op->name, ACC_NAME_SIZE);
			if (find_acc)
				break;
			acc_list_ptr = acc_list_ptr->next;
		}

		if (acc_list_ptr != NULL)
			acc_support = 1;
		else
			op->error_flag = -2;		// ACC is not supported
	}

	if (in_list || (!acc_support)) {
		return rc;
	}


	// Try to add a new entry in the accp list
	// In future, when time sharing is enabled, the ACC is always loaded

	if (op->port_id >= 0) {
		// Try to load ACC to the specified port
		if (acc_list_ptr->port_map [op->port_id] != 1) {
			printk ("%s do not support port %d\n", op->name, op->port_id);
			op->error_flag = -3;		// ACC port is not supported
		} else if (wukong.fpga_load->used_port[op->port_id]) {
			printk ("Can not load %s, port %d is busy.\n", op->name, op->port_id);
			op->error_flag = -4;		// The Wukong port is in used.
		}
	} else {
		// Try to load ACC to an idle port
		// We prefer the port that already been confgiured as the target ACC
		find_acc = -1;
		for (i = 0; i < MAX_ACC_PORT_NUM; i ++)
			if (acc_list_ptr->port_map [i]) {
				if (!wukong.fpga_load->used_port[i]) {
					find_acc = i;
					if (wukong.fpga_load->port_status [i].acc_id == acc_list_ptr->acc_info.acc_id)
						break;
				}
			}

		if (find_acc == -1) {
			printk ("Can not load %s, all ports are busy.\n", op->name);
			op->error_flag = -5;			// All ports are in used.
		} else
			op->port_id = find_acc;
	}

	acc_new = NULL;
	if ((op->port_id >= 0) && (op->error_flag == 0)) {
		char port_id_str [8];
		if (wukong.fpga_load->port_status[op->port_id].acc_id != acc_list_ptr->acc_info.acc_id) {
			if (acc_list_ptr->path [0] == '\0')
				printk ("Simulate reconfiguring %s into the port %d.\n", acc_list_ptr->acc_info.name, op->port_id);
			else {
				sprintf (port_id_str, "%d", op->port_id);
				printk ("Reconfiguring %s (%s) into the port %s.\n", acc_list_ptr->acc_info.name, acc_list_ptr->path, port_id_str);
				acc_support = reconfig_fpga (acc_list_ptr->path, port_id_str);
				if (acc_support != 0) {
					op->error_flag = -6;			// Reconfiguration Fail.
					op->port_id = -1;
					printk ("Reconfiguration fail. Error Code is %d\n", acc_support);
					return NULL;
				}
				printk ("Reconfiguration done.\n");
			}
		}

		if((acc_new = kmalloc(sizeof(struct acc_load_t), GFP_KERNEL)) == NULL) {
			printk(KERN_ERR "new accelerator info: out of memory\n");
			op->error_flag = -1;
		} else {
			wukong.fpga_load->used_port[op->port_id] = 1;
			wukong.fpga_load->port_status[op->port_id].acc_load = acc_new;
			wukong.fpga_load->port_status[op->port_id].acc_id = acc_list_ptr->acc_info.acc_id;
			acc_new->port_id = op->port_id;
			acc_new->limit_bw = acc_list_ptr->acc_info.max_bw;
			acc_new->acc_info = &(acc_list_ptr->acc_info);
			acc_new->acc_user = NULL;
			acc_new->next = NULL;
			spin_lock_init(& acc_new->acc_lock);

			if (acc_tail == accp)
				wukong.fpga_load->acc_load = acc_new;
			else
				acc_tail->next = acc_new;

			wukong.fpga_load->rsc -= acc_new->acc_info->rsc;
		}
	}

	return acc_new;
}


int remove_acc(unsigned long long acc_fd, unsigned long long acc_id, int port_id) {

	int rc;
	struct acc_load_t * accp, * acc_tail;

	if (NULL == wukong.fpga_load)
		return -1;

	accp = wukong.fpga_load->acc_load;
	acc_tail = accp;
	rc = 0;

	while (accp != NULL) {
		if((accp->port_id == port_id) && (accp->acc_info->acc_id == acc_id)) {
			if (accp->acc_user != NULL)
				break;

			// No user is openning the ACC, remove it from acc load list
			wukong.fpga_load->rsc += accp->acc_info->rsc;
			wukong.fpga_load->used_port [port_id] = 0;
			wukong.fpga_load->port_status [port_id].acc_load = NULL;
			// Here we do not clear the wukong.fpga_load->port_status [port_id].acc_id
			// because the ACC is still in FPGA untill being reconfigured

			if (accp == acc_tail)
				wukong.fpga_load->acc_load = accp->next;
			else
				acc_tail->next = accp->next;

			kfree(accp);
			break;
		}
	
		acc_tail = accp;
		accp = accp->next;
	}

	return rc;
}


ssize_t fpgacom_ioctl_acc_open(struct file *filp, struct ioctl_acc_open __user *arg) {
	int rc = 0;
	struct ioctl_acc_open op;
	struct acc_load_t *accp = NULL;
	struct acc_user_t *accu, *accu_tail, *acc_user_new = NULL;
	struct timeval tv;
	struct fpgacom_private_t * fpgacom_private = NULL, * fpgacom_pri_ptr;

	if (NULL == wukong.fpga_load) {
		return -EFAULT;
	}

	if(copy_from_user(&op, arg, sizeof(struct ioctl_acc_open)))
		return -EFAULT;

	if((fpgacom_private = kmalloc(sizeof(struct fpgacom_private_t), GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "ioctl_acc_open: out of memory\n");
		return -ENOMEM;
	}

	op.acc_fd = (unsigned long long) fpgacom_private;
	op.error_flag = 0;

	spin_lock(& wukong.fpga_load->fpga_lock);			// lock the fpga so that no one can remove an accelerator

	accp = load_acc (&op);

	if (accp == NULL) {
		op.port_id = -1;				// Can not find the ACC
		if(copy_to_user(arg, &op, sizeof(struct ioctl_acc_open)))
			rc = -EFAULT;
	} else {							// The ACC has been configured into FPGA
		op.port_id = accp->port_id;
		op.acc_id = accp->acc_info->acc_id;

		bw_control_array [op.port_id].bandwidth = accp->acc_info->max_bw;
		bw_control_array [op.port_id].ticket_span = (1000000*32/1024/bw_control_array [op.port_id].bandwidth);
		printk ("tick span on port %d is %ld us. bandwidth = %ld\n", op.port_id, bw_control_array [op.port_id].ticket_span, bw_control_array [op.port_id].bandwidth);

		if (wukong.fpga_load->port_status[accp->port_id].acc_id != accp->acc_info->acc_id) {
			printk ("Check acc load error.\n");
			rc = -EFAULT;
			goto acc_open_done;
		}

		// if the acc has been opened by the same process, the accu points to the data structure
		// if the acc has not been opened by the process, the accu is NULL
		accu = accp->acc_user;
		accu_tail = accu;
		while (accu != NULL) {
			if (accu->user_pid == current->tgid)
				break;

			accu_tail = accu;
			accu = accu->next;
		}

		if (accu != NULL)
			accu->count ++;
		else {
			if ((acc_user_new = kmalloc(sizeof(struct acc_user_t), GFP_KERNEL)) == NULL) {
				printk(KERN_ERR "acc_user_new: out of memory\n");
				rc = -ENOMEM;
				goto acc_open_done;
			}
		}

		if(copy_to_user(arg, &op, sizeof(struct ioctl_acc_open))) {
			rc = -EFAULT;
			goto acc_open_done;
		}

		if (accu == NULL) {
			acc_user_new->user_pid = current->tgid;
			acc_user_new->count = 1;
			acc_user_new->priority = (1<<16) | 1;
			acc_user_new->limited_bandwith = accp->acc_info->max_bw;
			acc_user_new->credit_left = (acc_user_new->limited_bandwith) << 20;
			do_gettimeofday (&tv);
			acc_user_new->time_us = tv.tv_sec*1000000 + tv.tv_usec;
			acc_user_new->history_payload = 0;
			acc_user_new->history_time = acc_user_new->time_us;
			acc_user_new->next = NULL;

			if (accp->acc_user == NULL)
				accp->acc_user = acc_user_new;
			else
				accu_tail->next = acc_user_new;

			accu = acc_user_new;
		}

		fpgacom_private->fpga_load = wukong.fpga_load;
		fpgacom_private->acc_load = accp;
		fpgacom_private->acc_user = accu;
		fpgacom_private->next = NULL;

		// One process might use the same linux fp to ioctl_acc_open multiple times, put the info
		// in the same link
		//
		// If one process uses different linux fp to ioctl_acc_open multiple times, the fpgacom_private
		// will be link to different filp->private_data
		fpgacom_pri_ptr = filp->private_data;
		if (NULL == fpgacom_pri_ptr)
			filp->private_data = fpgacom_private;
		else {
			while (NULL != fpgacom_pri_ptr->next)
				fpgacom_pri_ptr = fpgacom_pri_ptr->next;

			fpgacom_pri_ptr->next = fpgacom_private;
		}
	}

acc_open_done:
	if (rc < 0) {
		if (accp)
			remove_acc (op.acc_fd, op.acc_id, op.port_id);
		if (fpgacom_private)
			kfree (fpgacom_private);
		if (acc_user_new)
			kfree (acc_user_new);
	}

	spin_unlock(& wukong.fpga_load->fpga_lock);
	return rc;
}


ssize_t fpgacom_ioctl_acc_close(struct file *filp, struct ioctl_acc_open __user *arg) {
	int i;
	unsigned long long op;
	struct acc_load_t *accp;
	struct acc_user_t *accu = NULL, *accu_tail; 
	struct fpgacom_private_t * fpgacom_private = NULL, * fpgacom_pri_tail;

	if (NULL == wukong.fpga_load) {
		printk ("Warning. When closing the ACC, No fpga exist in system.\n");
		return -EFAULT;
	}

	accp = wukong.fpga_load->acc_load;
	if (NULL == accp) {
		printk ("Warning. When closing the ACC, No ACC exist in fpga.\n");
		return -EFAULT;
	}

	if(copy_from_user(&op, arg, sizeof(unsigned long long))) {
		return -EFAULT;
	}

	fpgacom_private = filp->private_data;
	fpgacom_pri_tail = fpgacom_private;
	if (NULL == fpgacom_private)
		printk ("Warning. When closing the ACC, none fpgacome private exist.\n");

	for (i = 0; fpgacom_private != NULL; i ++) {
		if ((unsigned long long)fpgacom_private == op) {
			if (i == 0) {
				filp->private_data = fpgacom_private->next;
				break;
			}

			fpgacom_pri_tail->next = fpgacom_private->next;
			break;
		}

		fpgacom_pri_tail = fpgacom_private;
		fpgacom_private = fpgacom_private->next;
	}

	if (fpgacom_private == NULL) {
		printk ("Warning. When closing the ACC, Can not find the matched fpgacom private to be closed.\n");
		return 0;
	}

	accp = fpgacom_private->acc_load;

	spin_lock(& accp->acc_lock);
	if (fpgacom_private->acc_user->count > 1)
		fpgacom_private->acc_user->count --;
	else {
		accu = accp->acc_user;
		accu_tail = accu;

		for (i = 0; accu != NULL; i ++) {
			if (accu == fpgacom_private->acc_user) {
				// When accu->count == 0, remove accu from the list
				if (i == 0) {
					accp->acc_user = accu->next;
					break;
				}

				accu_tail->next = accu->next;
				break;
			}

			accu_tail = accu;
			accu = accu->next;
		}

		if (accu)
			kfree(accu);
	}

	spin_unlock(& accp->acc_lock);

	spin_lock(& wukong.fpga_load->fpga_lock);
	remove_acc (op, accp->acc_info->acc_id, accp->port_id);
	spin_unlock(& wukong.fpga_load->fpga_lock);

	kfree (fpgacom_private);
	return 0;
}

extern void * result_virtaddr;
extern void * data_virtaddr;

ssize_t fpgacom_ioctl_acc_wdes(struct fpgacom_private_t * fpgacom_private, struct ioctl_acc_wdes __user *arg) {
	int rc = 0, i, j;
	struct ioctl_acc_wdes op;
	struct acc_user_t * accu;
	struct acc_descriptor * acc_des;
	struct timeval tv;
	unsigned long long timetrap;
	unsigned long pass_len;

	if(copy_from_user(&op, arg, sizeof(struct ioctl_acc_wdes))) {
		return -EFAULT;
	}

	accu = ((struct fpgacom_private_t *) (((struct acc_job_t *) op.key)->acc_fd))->acc_user;

	acc_des = (struct acc_descriptor *) op.key;
	if (NULL == acc_des) {
		printk ("Error. Kernel received a NULL descriptor.\n");
		return -EFAULT;
	}


	/*******************   Bandwidth Control  ******************/

	do_gettimeofday(&tv);
	timetrap = tv.tv_sec*1000000 + tv.tv_usec;
	if((timetrap - accu->time_us) > 1000000) {
		accu->history_payload = (accu->limited_bandwith << 20) - accu->credit_left;
		accu->history_time = timetrap - accu->time_us;
		accu->credit_left = accu->limited_bandwith << 20;
		accu->time_us = timetrap;
	}

	if ((timetrap - bw_control_array [acc_des->port_id].port_last_sec) > 1000000) {
		bw_control_array [acc_des->port_id].port_credit_left = bw_control_array [acc_des->port_id].bandwidth << 20;
		bw_control_array [acc_des->port_id].port_last_sec = timetrap;
	}

	/***********************************************************/


	if (! spin_trylock (& (bw_control_array [acc_des->port_id].port_trylock))) {
		op.ret = 0;
		if(copy_to_user(arg, &op, sizeof(struct ioctl_acc_wdes))) {
			rc = -EFAULT;
		}
		return rc;
	} else if(((unsigned long long)acc_des->data_len) > accu->credit_left) {
		op.ret = 0;
		spin_unlock (& (bw_control_array [acc_des->port_id].port_trylock));
		if(copy_to_user(arg, &op, sizeof(struct ioctl_acc_wdes))) {
			rc = -EFAULT;
		}
		return rc;
	} else if(((unsigned long)acc_des->data_len) > bw_control_array [acc_des->port_id].port_credit_left) {
		op.ret = 0;
		spin_unlock (& (bw_control_array [acc_des->port_id].port_trylock));
		if(copy_to_user(arg, &op, sizeof(struct ioctl_acc_wdes))) {
			rc = -EFAULT;
		}
		return rc;
	} else {

		if (0 == (accu->priority & 0xf0000000))
			acc_des->priority = accu->priority & 0xff;				// force the priority to the default value
		else if (acc_des->priority < ((accu->priority >> 16) & 0xff))
			acc_des->priority = (accu->priority >> 16) & 0xff;		// prevent the priority higher than the highest value

		if (acc_des->port_id == 0)
			acc_des->priority |= (1<<2);							// the acc is preemptalbe

		accu->credit_left -= (unsigned long long) acc_des->data_len;
		op.ret = accu->limited_bandwith;
	}


	/*******************  print acc descriptor  ****************/

//	unsigned int * q;
//	q = (unsigned int *) acc_des;
//	printk ("\nKernel received an ACC descriptor. des phy addr = 0x%016lx\n", (unsigned long long) __pa (op.key));
//	printk ("00-31 : %08x %08x %08x %08x %08x %08x %08x %08x.\n", q[0], q[1], q[2], q[3], q[4], q[5], q[6], q[7]);
//	printk ("32-63 : %08x %08x %08x %08x %08x %08x %08x %08x.\n\n", q[8], q[9], q[10], q[11], q[12], q[13], q[14], q[15]);

	/***********************************************************/

	do_gettimeofday(&tv);
	timetrap = tv.tv_sec*1000000 + tv.tv_usec;
	bw_control_array [acc_des->port_id].last_stamp = timetrap;
	bw_control_array [acc_des->port_id].len = 0;
	bw_control_array [acc_des->port_id].working = 1;

	j = 0;
	pass_len = 0;
	while (1) {
		do_gettimeofday(&tv);
		timetrap = tv.tv_sec*1000000 + tv.tv_usec;

		if (timetrap > host_stamp) {
			if (spin_trylock (&host_trylock)) {

				for (i = 0; i < 4; i ++) {
					if ((bw_control_array [bw_control_priority].working) && (timetrap > bw_control_array [bw_control_priority].last_stamp)) {
						bw_control_array [bw_control_priority].last_stamp = timetrap + bw_control_array [bw_control_priority].ticket_span - 10;
						pass_len += (32*1024);
						bw_control_array [bw_control_priority].port_credit_left -= (32*1024);
						bw_control_priority ++;
						bw_control_priority %= 4;
						break;
					} else {
						bw_control_priority ++;
						bw_control_priority %= 4;
					}
				}
				host_stamp = timetrap + host_span;
				spin_unlock (&host_trylock);
			}

			j ++;
		}

		if (pass_len >= acc_des->data_len) {
			bw_control_array [acc_des->port_id].working = 0;
			/************************  Simulation  *********************/
			acc_des->status = acc_des->data_len;		// Simulating the job is finished
			//memcpy (result_virtaddr, data_virtaddr, 64);
			/***********************************************************/
			break;
		}

		if (j == 100000) {
			printk ("time out exit in port %d, working len = %ld\n", acc_des->port_id, pass_len);
			break;
		}
	}

	spin_unlock (& (bw_control_array [acc_des->port_id].port_trylock));
	
	if(copy_to_user(arg, &op, sizeof(struct ioctl_acc_wdes))) {
		rc = -EFAULT;
	}
	
    return rc;
}		


ssize_t fpgacom_ioctl_bandwidth(struct ioctl_user_bandwidth __user *arg) {
	int rc = 0;
	struct ioctl_user_bandwidth op;
	struct acc_load_t *accp;
	struct acc_user_t *accu;

	if (NULL == wukong.fpga_load)
		return -EFAULT;

	accp = wukong.fpga_load->acc_load;
	if (NULL == accp)
		return -EFAULT;

	if(copy_from_user(&op, arg, sizeof(struct ioctl_user_bandwidth))) {
		return -EFAULT;
	}

	op.ret = -2;							// Do not find the ACC
	spin_lock(& wukong.fpga_load->fpga_lock);
	while (NULL != accp) {
		if (op.port_id == accp->port_id)
			break;

		accp = accp->next;
	}

	if (accp) {
		accu = accp->acc_user;
		while (NULL != accu) {
			if (op.user_pid == accu->user_pid)
				break;

			accu = accu->next;
		}

		if (accu) {
			accu->limited_bandwith = min(accp->acc_info->max_bw, op.bandwidth);
			op.ret = 0;
		} else
			op.ret = -1;					// Do not find the user
	}
	spin_unlock(& wukong.fpga_load->fpga_lock);

	if(copy_to_user(arg, &op, sizeof(struct ioctl_user_bandwidth)))
		rc = -EFAULT;
	
	return rc;
}


#if 0
ssize_t fpgacom_ioctl_user_priority(struct ioctl_user_priority __user *arg) {
	int rc = 0;
	struct ioctl_user_priority op;
	struct acc_info_t *accp;
	struct acc_user_t *accu;

	if (NULL == wukong.fpga_info)
		return -EFAULT;

	accp = wukong.fpga_info->acc_info;
	if (NULL == accp)
		return -EFAULT;

	if(copy_from_user(&op, arg, sizeof(struct ioctl_user_priority))) {
		return -EFAULT;
	}

	op.ret = -2;							// Do not find the ACC
	spin_lock(& wukong.fpga_info->fpga_lock);
	while (NULL != accp) {
		if (op.acc_id == accp->acc_id)
			break;

		accp = accp->next;
	}

	if (accp) {
		accu = accp->acc_user;
		while (NULL != accu) {
			if (op.user_pid == accu->user_pid)
				break;

			accu = accu->next;
		}

		if (accu) {
			op.ret = 0;

			if (op.priority & 0xf0000000)													// Set the highest priority, but not change current priority
				accu->priority = (op.priority & 0xffff0000) | ((op.priority >> 16) & 0xff);
			else if ((op.priority & 0xff) >= ((accu->priority >> 16) & 0xff))				// change current priority
				accu->priority = (accu->priority & 0x00ff0000) | (op.priority & 0xff);
			else
				op.ret = -3;

			op.priority = accu->priority;
		} else
			op.ret = -1;					// Do not find the user
	}
	spin_unlock(& wukong.fpga_info->fpga_lock);

	if(copy_to_user(arg, &op, sizeof(struct ioctl_user_priority)))
		rc = -EFAULT;
	
	return rc;
}

#ifdef VM_BLOCK_MEM

extern void *kpf_buf_addr;
extern void *pf_buf_addr;
unsigned long vm_addr_offset = 0;

ssize_t fpgacom_ioctl_vm_alloc(unsigned long __user *arg) {

	int rc;
	struct ioctl_vm_mem_addr vm_mem_addr;

	if(copy_from_user(&vm_mem_addr, arg, sizeof(struct ioctl_vm_mem_addr))) {
		return -EFAULT;
	}

	// for safe, a lock should be used here in case multi VM open at the same time
	vm_mem_addr.vm_hpa = ((unsigned long) kpf_buf_addr) + vm_addr_offset;
	vm_mem_addr.vm_hka = ((unsigned long) pf_buf_addr) + vm_addr_offset;
	vm_addr_offset += vm_mem_addr.vm_size;

	if(copy_to_user(arg, &vm_mem_addr, sizeof (struct ioctl_vm_mem_addr)))
		rc = -EFAULT;
	else
		rc = 0;

	return rc;
}

#endif
#endif

long fpgacom_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {

	switch(cmd) {
		case IOCTL_FPGACOM_LSACC:
			return fpgacom_ioctl_lsacc((void __user *)arg);
		case IOCTL_FPGACOM_LS:
			return fpgacom_ioctl_ls((void __user *)arg);
		case IOCTL_ACC_OPEN:
			return fpgacom_ioctl_acc_open(filp, (void __user *)arg);
		case IOCTL_ACC_CLOSE:
			return fpgacom_ioctl_acc_close(filp, (void __user *)arg);
		case IOCTL_ACC_WDES:
			return fpgacom_ioctl_acc_wdes((struct fpgacom_private_t * ) filp->private_data, (void __user *)arg);
		case IOCTL_FPGACOM_TOP:
			return fpgacom_ioctl_top((void __user *)arg);
		case IOCTL_USER_BANDWIDTH:
			return fpgacom_ioctl_bandwidth((void __user *) arg);
		case IOCTL_ACC_LIST:
			return fpgacom_update_acc_list((void __user *) arg);
/*		case IOCTL_USER_PRIORITY:
			return fpgacom_ioctl_user_priority((void __user *) arg);
#ifdef VM_BLOCK_MEM
		case IOCTL_VM_ALLOC:
			return fpgacom_ioctl_vm_alloc((void __user *)arg);
#endif
*/		default:
			return -ENOSYS;
	}

	return 0;
}

static int fpgacom_open( struct inode *inode, struct file *filp ){
//	printk( KERN_NOTICE"fpgacom device open!\n" );
	return 0;
}

static int fpgacom_release( struct inode *inode, struct file *filp ){
	int i;
	unsigned long long acc_fd;
	struct acc_load_t *accp;
	struct acc_user_t *accu = NULL, *accu_tail; 
	struct fpgacom_private_t * fpgacom_private = NULL, * fpgacom_private_next;

//	printk( KERN_NOTICE"fpgacom device close!\n" );

	fpgacom_private = filp->private_data;
	while (NULL != fpgacom_private) {
		acc_fd = (unsigned long long) fpgacom_private;
		accp = fpgacom_private->acc_load;
		printk ("Auto-release accp 0x%p\n", accp);

		spin_lock(& accp->acc_lock);
		if (fpgacom_private->acc_user->count > 1)
			fpgacom_private->acc_user->count --;
		else {
			accu = accp->acc_user;
			accu_tail = accu;

			for (i = 0; accu != NULL; i ++) {
				if (accu == fpgacom_private->acc_user) {
					// When accu->count == 0, remove accu from the list
					if (i == 0) {
						accp->acc_user = accu->next;
						break;
					}

					accu_tail->next = accu->next;
					break;
				}

				accu_tail = accu;
				accu = accu->next;
			}

			if (accu)
				kfree(accu);
		}

		spin_unlock(& accp->acc_lock);

		spin_lock(& wukong.fpga_load->fpga_lock);
		remove_acc (acc_fd, accp->acc_info->acc_id, accp->port_id);
		spin_unlock(& wukong.fpga_load->fpga_lock);

		fpgacom_private_next = fpgacom_private->next;
		kfree (fpgacom_private);
		fpgacom_private = fpgacom_private_next;
	}

	filp->private_data = NULL;
	return 0;
}


void fpgacom_vma_open(struct vm_area_struct *vma) {
//	printk(KERN_NOTICE "fpgacom VMA open, virt %lx, phys %lx\n", vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void fpgacom_vma_close(struct vm_area_struct *vma) {
//	printk(KERN_NOTICE "fpgacom VMA close\n");
}

static struct vm_operations_struct fpgacom_mmap_vm_ops = {
	.open  = fpgacom_vma_open,
	.close = fpgacom_vma_close,
};

#ifdef VM_BLOCK_MEM
static int fpgacom_mmap(struct file *filp, struct vm_area_struct *vma) {
	printk ("want to mmap 1G scceccd.\n");
	if(remap_pfn_range(vma, vma->vm_start, 0x100000000 >> PAGE_SHIFT, vma->vm_end - vma->vm_start, PAGE_SHARED))
		return -EAGAIN;

	printk ("mmap 1G scceccd.\n");

	vma->vm_ops = & fpgacom_mmap_vm_ops;
	fpgacom_vma_open(vma);

	return 0;
}
#else
static int fpgacom_mmap(struct file *filp, struct vm_area_struct *vma) {
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if(io_remap_pfn_range(vma, vma->vm_start, (fpga_dev.io_start >> PAGE_SHIFT), vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;

	vma->vm_ops = & fpgacom_mmap_vm_ops;
	fpgacom_vma_open(vma);

	return 0;
}
#endif

static struct file_operations fpgacom_ops = {
	.owner = THIS_MODULE,
	.open = fpgacom_open,
	.mmap	= fpgacom_mmap,
	.unlocked_ioctl = fpgacom_ioctl,
	.release = fpgacom_release,
};

struct fpga_load_t * fpgacom_setup_fpgainfo(void) {
	struct fpga_load_t * fpga_load = NULL;
	if((fpga_load = kmalloc(sizeof(struct fpga_load_t), GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "fpga_info: out of memory\n");
		return NULL;
	}

	memset (fpga_load->used_port, 0, MAX_ACC_PORT_NUM);
	memset (fpga_load->port_status, 0, MAX_ACC_PORT_NUM*sizeof(struct acc_port_status_t));
	fpga_load->rsc = 100;
	memset (fpga_load->fpga_info.port_map, 0, MAX_ACC_PORT_NUM);
	fpga_load->fpga_info.port_map [0] = 1;
	fpga_load->fpga_info.port_map [1] = 1;
	fpga_load->fpga_info.port_map [2] = 1;
	fpga_load->fpga_info.port_map [3] = 1;
	strcpy (fpga_load->fpga_info.vendor_name, "Xilinx");
	strcpy (fpga_load->fpga_info.fpga_name, "Virtex 5 115T");
	strcpy (fpga_load->fpga_info.platform_name, "IBM Wukong");
	fpga_load->fpga_info.platform_id = 0xe001;
	fpga_load->acc_load = NULL;
	fpga_load->next = NULL;
	spin_lock_init(& fpga_load->fpga_lock);
	return fpga_load;
}


/* set up the cdev stucture for a device */
static void fpgacom_setup_cdev( struct cdev *dev, int minor, struct
file_operations *fops ){
	int err;
	int devno = MKDEV(fpgacom_major, minor);
	/* initialize the cdev struct */
	cdev_init(dev,fops);
	dev->owner = THIS_MODULE;
	dev->ops = fops;
	err = cdev_add(dev, devno, 1); /* register the cdev in the kernel */
	if(err)
		printk( KERN_NOTICE"Error %d adding fpgacom%d\n",err ,minor );
}


int fpgacom_register_test (void)
{
	return 0;
}


/* Module housekeeping */
static int __init fpgacom_init(void){
	int ret;
	dev_t dev = MKDEV( fpgacom_major, fpgacom_minor );

	/* alloc the major	device number dynamicly */
	ret = alloc_chrdev_region(&dev, fpgacom_minor , 1, "fpgacom" );
	if( ret < 0 ){
		printk( KERN_NOTICE"fpgacom: unable to get major %d\n",fpgacom_major );
		return ret;
	}
	fpgacom_major = MAJOR(dev);
	/* set up devices, in this case, there is only one device */
	printk( KERN_NOTICE"fpgacom init: %d, %d\n",fpgacom_major,fpgacom_minor );
	fpgacom_setup_cdev(&fpgacomDev, fpgacom_minor , &fpgacom_ops );

	fpgacom_class = class_create(THIS_MODULE, "fpgacom_class");
	if(IS_ERR(fpgacom_class)) {
		printk("Error: failed in creating class.\n");
		return -1;
	}

	device_create(fpgacom_class, NULL, MKDEV(fpgacom_major, fpgacom_minor), NULL, "fpgacom");

	ret = fpga_setup();

	if (ret) {
		unregister_chrdev_region( MKDEV( fpgacom_major, fpgacom_minor ), 1 );
		device_destroy(fpgacom_class, MKDEV(fpgacom_major, fpgacom_minor));
		class_destroy(fpgacom_class);
		return ret;
	}

	ret = fpgacom_register_test();
	if (ret) {
		unregister_chrdev_region( MKDEV( fpgacom_major, fpgacom_minor ), 1 );
		device_destroy(fpgacom_class, MKDEV(fpgacom_major, fpgacom_minor));
		class_destroy(fpgacom_class);
		fpga_unsetup();
		return -1;
	}

	wukong.fpga_load = fpgacom_setup_fpgainfo();
	spin_lock_init(& wukong.wukong_lock);
	spin_lock_init(& host_trylock);
	memset (&bw_control_array, 0, sizeof (bw_control_array));
	spin_lock_init(& (bw_control_array[0].port_trylock));
	spin_lock_init(& (bw_control_array[1].port_trylock));
	spin_lock_init(& (bw_control_array[2].port_trylock));
	spin_lock_init(& (bw_control_array[3].port_trylock));
	create_acc_list ();

	chmod_node(1);
	chmod_node(0);
	return ret;
}

/* Exit routine */
static void __exit fpgacom_exit(void){
	struct acc_load_t * accp, * acc_next;
	struct acc_list_entry_t * acc_list_ptr, * acc_list_next_ptr;

	/* remove the cdev from kernel */
	cdev_del(&fpgacomDev );

	/* release the device numble alloced earlier */
	unregister_chrdev_region( MKDEV( fpgacom_major, fpgacom_minor ), 1 );
	printk( KERN_NOTICE"fpgacom exit. major:%d,minor %x\n",fpgacom_major,fpgacom_minor );

	accp = wukong.fpga_load->acc_load;
	while(NULL != accp) {
		acc_next = accp->next;
		kfree(accp);
		accp = acc_next;
	}

	acc_list_ptr = acc_list;
	while (acc_list_ptr != NULL) {
		acc_list_next_ptr = acc_list_ptr->next;
		kfree(acc_list_ptr);
		acc_list_ptr = acc_list_next_ptr;
	}
	acc_list = NULL;

	device_destroy(fpgacom_class, MKDEV(fpgacom_major, fpgacom_minor));
	class_destroy(fpgacom_class);

	//exit fpga driver
	fpga_unsetup();
}

module_init(fpgacom_init);

module_exit(fpgacom_exit);
