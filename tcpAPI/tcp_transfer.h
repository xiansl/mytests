#ifndef TCP_TRANSFER_H
#define TCP_TRANSFER_H
#include <stdio.h>
#include <stdlib.h>	/* needed for os x*/
#include <string.h>	/* for strlen */
#include <netdb.h>      /* for gethostbyname() */
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h> 
#include <sys/errno.h>   /* defines ERESTART, EINTR */
#include <sys/wait.h>    /* defines WNOHANG, for wait() */
#include <unistd.h>
#include <arpa/inet.h>	/* for printing an internet address in a user-friendly way */
#include <pthread.h>
#include <memory.h>
#define DEBUG 1
#define BACKLOG 5
#define MTU 4096
#define MIN(a, b) ((a)>(b)?(b):(a))
#define MAX_BUFFER_SIZE (1024 * sysconf(_SC_PAGE_SIZE))

#define TEST_NEG(x) do { if ( (x) < 0 ) exit(1); } while (0)
#define TEST_NZ(x) do { if ( (x)) die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) die("error: " #x " failed (returned zero/null)."); } while (0)

struct socket_context_t {
    char server_host[16];
    char scheduler_host[16];
    int server_port;
    int scheduler_port;
    int to_scheduler_fd;
    int to_server_fd;
    void * scheduler_addr;
    void * to_scheduler_addr;
    void * server_addr;
    void * to_server_addr;

    char job_id[16];
	char section_id[16];
	char status[16];
    void * in_buf;
    void * out_buf;
};
struct server_context_t {
	char section_id[16];
	char acc_name[16]; 
    unsigned int in_buf_size;
    unsigned int out_buf_size;
    void * in_buf;
    void * out_buf;
    void * acc_handler;
    void * server_addr;
    void * server_fd;

};
struct client_to_scheduler{
    char status[16];//"open" or "close";
    char real_in_size[16];
    char in_buf_size[16];
    char out_buf_size[16];
    char acc_name[16];
    char job_id[16];
};

struct scheduler_to_client{
    char host[16];
    char port[16];
    char section_id[16];
    char status[16];
    char job_id[16];
};

void build_context(void *acc_context, char *acc_name, unsigned int in_buf_size, unsigned int out_buf_size, const char *scheduler_host, int scheduler_port);
int build_connection_to_scheduler(void *acc_context);
int request_to_scheduler(void *acc_context);
int build_connection_to_server(void *acc_context);
unsigned int remote_acc_do_job(void *acc_ctx, const char *param, unsigned int job_len, void ** result_buf);
void disconnect_with_server(void *acc_ctx);
void report_to_scheduler(void *acc_ctx);
void free_memory(void *acc_ctx);
void socket_server_open(void *server_param);
void * tcp_server_data_transfer(void * server_param);

int local_fpga_open(void * server_param);
unsigned long local_fpga_do_job(void * server_context);
void local_fpga_close(void * server_context);
size_t send_msg(int fd, void *buffer, size_t len, size_t chunk);
size_t recv_msg(int fd, void *buffer, size_t len, size_t chunk);
#endif
