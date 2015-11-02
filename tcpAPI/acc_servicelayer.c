#include <memory.h>
#include "tcp_transfer.h"
#include "acc_servicelayer.h"


void * build_socket_context(void *acc_context, char *host, int port);
int build_connection_to_scheduler(void *scheduler_socket_context);
int build_connection_to_server(void *server_socket_context);



void * fpga_acc_open(struct acc_context_t *acc_context, char *acc_name, unsigned int in_buf_size, unsigned int out_buf_size, char *scheduler_host, int scheduler_port){
    build_context((void *)acc_context, acc_name, in_buf_size, out_buf_size, scheduler_host, scheduler_port);
    int to_scheduler_fd, to_server_fd, status;

    struct socket_context_t *socket_ctx = acc_context->socket_context;
    to_scheduler_fd = build_connection_to_scheduler((void *)acc_context);
    TEST_NEG(to_scheduler_fd);
    status = request_to_scheduler((void *)acc_context);

    switch(status){
        case 0: {//remote ACC is available
            if (DEBUG)
                printf("open a remote acc...\n");
            to_server_fd = build_connection_to_server((void *)acc_context);
            TEST_NEG(to_server_fd);
            break;
                }
        case 1:{
            printf("Fail to open ACC: remote server not responding\n.");
            TEST_NEG(-1);
            break;
               }
        case 2:{
            printf("Fail to open ACC: remote acc not available\n.");
            TEST_NEG(-1);
            break;
               }
        case 3:{ //local ACC is available
            if (DEBUG)
                printf("open a local acc...\n");
            int section_id = atoi(socket_ctx->section_id);
            printf("in_buf_size=%u, out_buf_size=%u, section_id=%d\n", acc_context->in_buf_size, acc_context->out_buf_size, section_id);
            acc_context->in_buf = pri_acc_open(&(acc_context->acc_handler), acc_context->acc_name, acc_context->in_buf_size, acc_context->out_buf_size, section_id);
            socket_ctx->in_buf = acc_context->in_buf;
            break;
               }
        case '?':{
            if (DEBUG)
                printf("Error: unknown status.\n");
            break;
                 }
    }
    return acc_context->in_buf;
}

unsigned long fpga_acc_do_job (struct acc_context_t * acc_context, const char * param, unsigned int job_len, void ** result_buf){
    struct socket_context_t * socket_ctx = (struct socket_context_t * ) (acc_context->socket_context);
    unsigned long result_buf_size;
    int status = atoi(socket_ctx->status);
    switch (status){
        case 0 :{//remote ACC is available;
            printf("do REMOTE Job...\n");
            result_buf_size = remote_acc_do_job((void *)acc_context, param, job_len, result_buf); 
            break;
        }
        case 3 :{//local ACC is available;
            if (DEBUG)
                printf("do LOCAL job...\n");
            result_buf_size = acc_do_job(&(acc_context->acc_handler), param, job_len, result_buf); 
            break;
}           
        case '?':
            break;
    }
    printf("result buf size=%ld\n", result_buf_size);
    return result_buf_size;
}
 
void fpga_acc_close(struct acc_context_t * acc_context) {
    struct socket_context_t *socket_ctx = (struct socket_context_t *)acc_context->socket_context;
    int status = atoi(socket_ctx->status); 
    switch (status){
        case 3:{
            printf("close loal acc\n");
            acc_close(&(acc_context->acc_handler));
            break;
            }
        case 0:{
            printf("close remote acc\n");
            disconnect_with_server((void *)acc_context);
            break;
            }
        case '?':{
            printf("Error: unknown status.\n");
            break;
            }
    }
    report_to_scheduler((void *)acc_context);
    free_memory((void *)acc_context);
    
}

void disconnect_with_server(void *acc_ctx){
    struct acc_context_t *acc_context = (struct acc_context_t *) acc_ctx;
    struct socket_context_t *socket_ctx = (struct socket_context_t *)acc_context->socket_context;
    int fd = socket_ctx->to_server_fd;
    //send(fd, in_buf, 0, 0);
    close(fd);
    return;
}

void report_to_scheduler(void *acc_ctx){
    return;
}

