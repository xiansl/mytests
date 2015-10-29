#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <rdma/rdma_cma.h>
#include <net/if.h>
#include <fcntl.h>
#include <libgen.h>

#include "acc_servicelayer.h"

#define TEST_NZ(x) do { if ( (x)) die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) die("error: " #x " failed (returned zero/null)."); } while (0)
char * get_local_ip_addr(void);
void client_rdma_setup_connection(void *acc_context);

unsigned long client_rdma_data_transfer (void *acc_context, void **result_buf);

void client_rdma_disconnect (void *acc_context);
