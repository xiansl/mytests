
struct fpga_dev_t {
	struct pci_dev		*pdev;
	unsigned int		io_start;
	unsigned int		io_end;
	unsigned int		io_flags;
	unsigned int		io_len;
};


int fpga_setup(void);
void fpga_unsetup(void);

