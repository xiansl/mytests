#include <linux/pci.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kmod.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <asm/uaccess.h>
#include <asm/ioctl.h>
#include "fpgacom.h"
#include "fpgadev.h"

#define PCI_VENDOR_ID_FPGA 0x1014
#define PCI_DEVICE_ID_FPGA 0x3555
#define FPGA_MAJOR 0
#define DEVNODE_COUNT 50

spinlock_t fpgadev_lock;
static unsigned int global_des_num = 0;
void * result_virtaddr;
void * data_virtaddr;

struct fpga_private_t {
	int i;
	dma_addr_t dma_data_busaddr;
	dma_addr_t dma_result_busaddr;
	dma_addr_t dma_des_busaddr;
	unsigned int data_size;
	unsigned int result_size;
	void * dma_data_virtaddr;
	void * dma_result_virtaddr;
	void *  dma_des_virtaddr;
	struct fpga_private_t * next;
};

char fpga_driver_name[] = "fpga";
int fpga_major = FPGA_MAJOR;
int fpga_minor = 0;
volatile unsigned int *ioremap_addr = NULL;
static struct cdev cdev;
struct class *fpga_class = NULL;

MODULE_LICENSE("Dual BSD/GPL");

struct fpga_dev_t fpga_dev = {NULL, 0, 0, 0, 0};

static struct pci_device_id fpga_pci_tbl[] = {
	{
		PCI_VENDOR_ID_FPGA, PCI_DEVICE_ID_FPGA,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0
	},

	{0,}
};

MODULE_DEVICE_TABLE(pci, fpga_pci_tbl);


void * fpga_buffer_malloc (struct fpga_private_t * fpga_private, unsigned int size, unsigned long * bus_addr) {
//	return pci_alloc_consistent(fpga_dev.pdev, size, bus_addr);
//	return dma_alloc_coherent(& fpga_dev.pdev->dev, size, bus_addr, GFP_DMA | GFP_USER);
	void * virt_addr;
	unsigned int i;
	void * addr;

	virt_addr = (void *) __get_free_pages (GFP_USER, get_order(size));
	(* bus_addr) = __pa(virt_addr);

	if (virt_addr == NULL)
		return virt_addr;

	// If the bus_addr is not 4KB align
	if ((*bus_addr) & 0xfffUL) {
		printk ("Error. Buffer is not 4KByte aligned\n");
		free_pages ((unsigned long)virt_addr, get_order(size));
		return NULL;
	}

	i = size;
	addr = virt_addr;
	while (i > 0) {
		SetPageReserved(virt_to_page(addr));
		addr += PAGE_SIZE;
		i -= PAGE_SIZE;
	}

	return virt_addr;
}

void fpga_buffer_free (struct fpga_private_t * fpga_private, unsigned int size, unsigned long virt_addr, unsigned long bus_addr) {

	unsigned int i;
	void * addr;
//	pci_free_consistent (fpga_dev.pdev, size, virt_addr, bus_addr); return;

	i = size;
	addr = (void *) virt_addr;
	while (i > 0) {
		ClearPageReserved(virt_to_page(addr));
		addr += PAGE_SIZE;
		i -= PAGE_SIZE;
	}

	free_pages (virt_addr, get_order(size));
	return;
}


static int fpga_open(struct inode *inode, struct file *filp) {
	return 0;
}



long fpga_ioctl_acc_malloc (struct file *filp, struct ioctl_acc_wdes __user *arg) {
	struct ioctl_acc_malloc acc_malloc_req;
	struct acc_descriptor * des;
	struct acc_job_t * acc_job;
	struct fpga_private_t * fpga_private, * private_ptr;
	unsigned long hpa_offset;

	spin_lock(& fpgadev_lock);
	if (global_des_num == MAX_DES_NUM) {
		spin_unlock(& fpgadev_lock);
		return -EFAULT;
	} else
		spin_unlock(& fpgadev_lock);

	if(copy_from_user(&acc_malloc_req, arg, sizeof(struct ioctl_acc_malloc)))
		return -EFAULT;
	
	if((fpga_private = kmalloc(sizeof(struct fpga_private_t), GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "kmalloc struct fpga_private_t: out of memory\n");
		return -ENOMEM;
	}

	fpga_private->i = 0;
	fpga_private->dma_data_virtaddr = NULL;
	fpga_private->dma_result_virtaddr = NULL;
	fpga_private->dma_des_virtaddr = NULL;
	fpga_private->next = NULL;

	fpga_private->dma_data_virtaddr = fpga_buffer_malloc (fpga_private, acc_malloc_req.data_size, (unsigned long *) & fpga_private->dma_data_busaddr);
	if (NULL == fpga_private->dma_data_virtaddr) {
		printk ("Kmalloc 0x%x bytes for source data out of memory.\n", acc_malloc_req.data_size);
		goto malloc_fail_0;
	}

	fpga_private->dma_result_virtaddr = fpga_buffer_malloc (fpga_private, acc_malloc_req.result_size, (unsigned long *) & fpga_private->dma_result_busaddr);
	if (NULL == fpga_private->dma_result_virtaddr) {
		printk ("Kmalloc 0x%x bytes for result out of memory.\n", acc_malloc_req.result_size);
		goto malloc_fail_1;	
	}

	fpga_private->dma_des_virtaddr = fpga_buffer_malloc (fpga_private, PAGE_SIZE, (unsigned long *) & fpga_private->dma_des_busaddr);
	if (NULL == fpga_private->dma_des_virtaddr) {
		printk ("Kmalloc %ld bytes for ACC descriptor out of memory.\n", PAGE_SIZE);
		goto malloc_fail_2;
	}

	acc_malloc_req.key = (unsigned long long) ((unsigned long) fpga_private->dma_des_virtaddr);
	if(copy_to_user(arg, &acc_malloc_req, sizeof(struct ioctl_acc_malloc)))
		goto malloc_fail_3;

	hpa_offset = 0;

	fpga_private->data_size = acc_malloc_req.data_size;
	fpga_private->result_size = acc_malloc_req.result_size;
	printk ("\nAll DMA buffer is ready. data phy addr = 0x%lx, result phy addr = 0x%lx, des phy addr = 0x%lx\n", (unsigned long)fpga_private->dma_data_busaddr, (unsigned long) fpga_private->dma_result_busaddr, (unsigned long) fpga_private->dma_des_busaddr);

	printk ("data virt addr = %p, result virt addr = %p, des virt addr = %p.\n", fpga_private->dma_data_virtaddr, fpga_private->dma_result_virtaddr, fpga_private->dma_des_virtaddr);

	// add the new struct to the list
	private_ptr = (struct fpga_private_t *) filp->private_data;
	if (NULL == private_ptr)
		filp->private_data = fpga_private;
	else {
		while (NULL != private_ptr->next) {
			private_ptr = private_ptr->next;
		}

		private_ptr->next = fpga_private;
	}

	des = (struct acc_descriptor *) fpga_private->dma_des_virtaddr;
	des->priority = 1;
	des->port_id = ((struct fpgacom_private_t *) acc_malloc_req.acc_fd)->acc_load->port_id;
	des->data_len = fpga_private->data_size;
	des->data_addr = hpa_offset + fpga_private->dma_data_busaddr;
	des->result_len = fpga_private->result_size;
	des->result_addr = hpa_offset + fpga_private->dma_result_busaddr;
	des->des_addr = hpa_offset + fpga_private->dma_des_busaddr;

	acc_job = (struct acc_job_t *) fpga_private->dma_des_virtaddr;
	acc_job->acc_fd = acc_malloc_req.acc_fd;
	result_virtaddr = fpga_private->dma_result_virtaddr;
	data_virtaddr = fpga_private->dma_data_virtaddr;

	spin_lock(& fpgadev_lock);
	global_des_num ++;
	spin_unlock(& fpgadev_lock);
	return 0;

malloc_fail_3:
	fpga_buffer_free (fpga_private, PAGE_SIZE, (unsigned long) fpga_private->dma_des_virtaddr, (unsigned long) fpga_private->dma_des_busaddr);
malloc_fail_2:
	fpga_buffer_free (fpga_private, acc_malloc_req.result_size, (unsigned long) fpga_private->dma_result_virtaddr, (unsigned long) fpga_private->dma_result_busaddr);
malloc_fail_1:
	fpga_buffer_free (fpga_private, acc_malloc_req.data_size, (unsigned long) fpga_private->dma_data_virtaddr, (unsigned long) fpga_private->dma_data_busaddr);
malloc_fail_0:
	kfree (fpga_private);
	return -EFAULT;
}


long fpga_ioctl_acc_free (struct file *filp, unsigned long __user *arg) {

	unsigned long long key;
	struct fpga_private_t * fpga_private, * private_ptr = NULL;

	if(copy_from_user(&key, arg, sizeof(unsigned long long))) {
		printk ("Get the key from acc free fail.\n");
		return -EFAULT;
	}
	
	fpga_private = (struct fpga_private_t *) filp->private_data;
	while (NULL != fpga_private) {
		if (key == ((unsigned long long) ((unsigned long)fpga_private->dma_des_virtaddr)))
			break;

		private_ptr = fpga_private;
		fpga_private = fpga_private->next;
	}

	if (NULL == fpga_private)
		return -EFAULT;

	fpga_buffer_free (fpga_private, fpga_private->data_size, (unsigned long) fpga_private->dma_data_virtaddr, (unsigned long) fpga_private->dma_data_busaddr);
	fpga_buffer_free (fpga_private, fpga_private->result_size, (unsigned long) fpga_private->dma_result_virtaddr, (unsigned long) fpga_private->dma_result_busaddr);
	fpga_buffer_free (fpga_private, PAGE_SIZE, (unsigned long) fpga_private->dma_des_virtaddr, (unsigned long) fpga_private->dma_des_busaddr);

	if (fpga_private == (struct fpga_private_t *) filp->private_data)
		filp->private_data = fpga_private->next;
	else
		private_ptr->next = fpga_private->next;

	kfree (fpga_private);
	spin_lock(& fpgadev_lock);
	if (global_des_num)
		global_des_num --;
	else
		printk ("Error. FPGA global_des_num underflow.\n");
	spin_unlock(& fpgadev_lock);
	return 0;
}


long fpga_ioctl (struct file *filp, unsigned int cmd, unsigned long arg) {
	switch (cmd) {
		case IOCTL_ACC_MALLOC:
			return fpga_ioctl_acc_malloc (filp, (void __user *)arg);
		case IOCTL_ACC_FREE:
			return fpga_ioctl_acc_free (filp, (void __user *)arg);
		default:
			return -ENOSYS;
	}
}


static ssize_t fpga_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
	return 0;
}

static ssize_t fpga_write(struct file *filp, const char __user *buf,size_t count, loff_t *f_pos) {
	return 0;
}

void fpga_vma_open(struct vm_area_struct *vma) {
//	printk(KERN_NOTICE "fpga VMA open, virt %lx, phys %lx\n", vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void fpga_vma_close(struct vm_area_struct *vma) {
//	printk(KERN_NOTICE "fpga VMA close\n");
}

static struct vm_operations_struct fpga_remap_vm_ops = {
	.open  = fpga_vma_open,
	.close = fpga_vma_close,
};

static int fpga_remap(struct file *filp, struct vm_area_struct *vma) {
	struct fpga_private_t *fpga_private = filp->private_data;

	if (NULL == fpga_private) {
		printk ("filp->private_data should not be NULL when in mmap.\n");
		return -EFAULT;
	}

	while (NULL != fpga_private->next)			// find the newest fpga_private data;
		fpga_private = fpga_private->next;

	fpga_private->i %= 3;
	switch(fpga_private->i) {
		printk("mmap at the %d time\n", fpga_private->i);
		case 0:
		{
			if(remap_pfn_range(vma, vma->vm_start, fpga_private->dma_data_busaddr >> PAGE_SHIFT, vma->vm_end - vma->vm_start, PAGE_SHARED))
				return -EAGAIN;
			printk("mmap source data succeed. phyaddr = 0x%lx. length = 0x%08x.\n", (unsigned long) fpga_private->dma_data_busaddr, (unsigned int) (vma->vm_end - vma->vm_start));
		}
		break;
	
		case 1:
		{
			if(remap_pfn_range(vma, vma->vm_start, fpga_private->dma_result_busaddr >> PAGE_SHIFT, vma->vm_end - vma->vm_start, PAGE_SHARED))
				return -EAGAIN;
			printk("mmap result      succeed. phyaddr = 0x%lx. length = 0x%08x.\n", (unsigned long) fpga_private->dma_result_busaddr, (unsigned int) (vma->vm_end - vma->vm_start));
		}
		break;
	
		case 2:
		{
			vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
			if(remap_pfn_range(vma, vma->vm_start, fpga_private->dma_des_busaddr >> PAGE_SHIFT, vma->vm_end - vma->vm_start, PAGE_SHARED))
				return -EAGAIN;
			printk("mmap descriptor  succeed. phyaddr = 0x%lx. length = 0x%08x.\n", (unsigned long) fpga_private->dma_des_busaddr, (unsigned int) (vma->vm_end - vma->vm_start));
		}
		break;

		default:
			return -ENOSYS;
	}

	fpga_private->i +=1;
	vma->vm_ops = & fpga_remap_vm_ops;
	fpga_vma_open(vma);

	return 0;

}

static int fpga_release(struct inode *inode, struct file *filp) {
	struct fpga_private_t *fpga_private = filp->private_data, * fpga_private_next;
	unsigned long long key;

	while (NULL != fpga_private) {
		key = (unsigned long long) fpga_private->dma_des_virtaddr;
		printk ("Auto-release the buffers with key = 0x%llx\n", key);

		fpga_buffer_free (fpga_private, fpga_private->data_size, (unsigned long) fpga_private->dma_data_virtaddr, (unsigned long) fpga_private->dma_data_busaddr);

		fpga_buffer_free (fpga_private, fpga_private->result_size, (unsigned long) fpga_private->dma_result_virtaddr, (unsigned long) fpga_private->dma_result_busaddr);

		fpga_buffer_free (fpga_private, PAGE_SIZE, (unsigned long) fpga_private->dma_des_virtaddr, (unsigned long) fpga_private->dma_des_busaddr);
		fpga_private_next = fpga_private->next;
		kfree(fpga_private);

		spin_lock(& fpgadev_lock);
		if (global_des_num)
			global_des_num --;
		else
			printk ("Error. FPGA global_des_num underflow.\n");
		spin_unlock(& fpgadev_lock);

		fpga_private = fpga_private_next;
	}

	filp->private_data = NULL;
	return 0;
}

static struct file_operations fpga_ops = {
	.owner   = THIS_MODULE,
	.open    = fpga_open,
	.read    = fpga_read,
	.write   = fpga_write,
	.release = fpga_release,
	.mmap    = fpga_remap,
	.unlocked_ioctl = fpga_ioctl,
};

static void fpga_setup_cdev(struct cdev *cdev, int minor, struct file_operations *fops) {
	int err;
	dev_t devno = MKDEV(fpga_major, minor);
	cdev_init(cdev, fops);
	cdev->owner = THIS_MODULE;
	cdev->ops = &fpga_ops;
	err = cdev_add(cdev, devno, DEVNODE_COUNT);
	if(err)
		printk(KERN_ERR "Error %d adding fpga%d\n", err, minor);
}

static int fpga_create(void) {
	int result = 0;

	dev_t devno = MKDEV(fpga_major, fpga_minor);

	if(fpga_major)
		result = register_chrdev_region(devno, DEVNODE_COUNT, "FPGA");
	else 
		result = alloc_chrdev_region(&devno, fpga_minor, DEVNODE_COUNT, "FPGA");
	
	if(result < 0) {
		printk(KERN_ERR "fpga: unable to get major\n");
		return result;
	}

	fpga_major = MAJOR(devno);
	fpga_setup_cdev(&cdev, fpga_minor, &fpga_ops);
	fpga_class = class_create(THIS_MODULE, "fpga_class");
	if(IS_ERR(fpga_class)) {
		printk("Error: failed in creating class.\n");
		return -1;
	}

	device_create(fpga_class, NULL, devno, NULL, "fpga");
	return 0;	
}


static void fpga_remove(void) {
	dev_t devno = MKDEV(fpga_major, fpga_minor);
	unregister_chrdev_region(devno, DEVNODE_COUNT);
	device_destroy(fpga_class, devno);
	class_destroy(fpga_class);
}

int fpga_setup(void) {

	if (fpga_create()) {
		printk ("Can't not create /dev/fpga\n");
		return -1;
	}

	spin_lock_init(& fpgadev_lock);
	return 0;
}

void fpga_unsetup(void) {

	fpga_remove();
	printk(KERN_INFO "fpga unsetup\n");
}

