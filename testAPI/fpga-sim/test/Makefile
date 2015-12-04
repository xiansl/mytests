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
LDLIBS  += -lfpga
LDFLAGS += -pthread
LDFLAGS += -L../driver
#If the LD_PRELOAD flag is not used, just enable the following 3 lines.

tinybench:
	$(CC) $(INCLUDES) -D_GNU_SOURCE -o tinybench tinyBench.c workThd.c $(LDFLAGS) $(LDLIBS)

micro:
	$(CC) $(INCLUDES) -D_GNU_SOURCE -o micro_test micro_test.c $(LDFLAGS) $(LDLIBS)
micro1:
	$(CC) $(INCLUDES) -D_GNU_SOURCE -o micro_test1 micro_test1.c $(LDFLAGS) $(LDLIBS)

debug:
	$(CC) $(INCLUDES) -g -o tinybench *.c $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) *.o *~ tinybench micro_test
