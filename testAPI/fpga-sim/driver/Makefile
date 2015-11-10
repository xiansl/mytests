#DEBUG = y

# Add your debugging flag (or not) to EXTRA_CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)

# 如果已经定义KERNELRELEASE，则说明是从内核构造系统调用的

# 因此可以利用其内建语句

ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m	:= fpga.o
fpga-objs :=fpgacom.o fpgadev.o

# 否则，是直接从命令行调用的

# 这时要调用内核构造系统
else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD)  modules

endif



clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend


TEST_SOURCES = test_hello.c
.PHONY: test
test:$(TEST_SOURCES)
	$(CC) $< -o $@

ifeq (.depend,$(wildcard .depend))
include .depend
endif
