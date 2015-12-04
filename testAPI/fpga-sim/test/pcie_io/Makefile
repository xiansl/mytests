#
# If PowerEN headers & libraries exist in $(PATH_blu),
# then build with the following:
#
#   LDFLAGS=-L$PATH_blu INCLUDES=-I$PATH_blu make
#

#
# Prefix for a CROSS toolchain, if any.
#
# Examples:
#   CROSS=/opt/cell/toolchain/bin/ppu-
#   CROSS=powerpc64-unknown-linux-gnu-

ifeq ($(shell uname -m), ppc64)
CROSS=
else
CROSS=
endif

CC=$(CROSS)gcc
CPPFLAGS += $(INCLUDES)
LD=$(CROSS)ld
AR=ar
#INCLUDES=-I ../../usr/include/

LDLIBS  += -lpthread
LDLIBS  += -ldl
LDLIBS  += -lm
LDFLAGS += -pthread
#If the LD_PRELOAD flag is not used, just enable the following 3 lines.

tinybench:
	$(CC) $(INCLUDES) -D_GNU_SOURCE -o pcie_io *.c $(LDFLAGS) $(LDLIBS) #-L../fpgacom/ -lfpga-libacc -ldl

debug:
	$(CC) $(INCLUDES) -g -o pcie_io *.c $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) *.o *~ pcie_io
