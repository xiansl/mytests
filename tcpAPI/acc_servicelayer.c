#include <memory.h>
#include "tcp_transfer.h"
#include "acc_servicelayer.h"

int build_connection_to_scheduler(void *acc_context);
void report_to_scheduler(void *acc_ctx);


void * fpga_acc_open(struct acc_context_t *acc_context, char *acc_name, unsigned int in_buf_size, unsigned int out_buf_size, char *scheduler_host, int scheduler_port){

    struct timeval t1, t2, dt;
    gettimeofday(&t1, NULL);
    build_context((void *)acc_context, acc_name, in_buf_size, out_buf_size, scheduler_host, scheduler_port);
    int to_scheduler_fd, to_server_fd, status;

    struct socket_context_t *socket_ctx = acc_context->socket_context;

    to_scheduler_fd = build_connection_to_scheduler((void *)acc_context);
    TEST_NEG(to_scheduler_fd);
    status = request_to_scheduler((void *)acc_context);

    switch(status){
        case 0: {//remote ACC is available
            if (DEBUG)
                printf("open a REMOTE Acc...\n");
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
                printf("open a LOCAL Acc...\n");
            int section_id = atoi(socket_ctx->section_id);
            //printf("in_buf_size=%u, out_buf_size=%u, section_id=%d\n", acc_context->in_buf_size, acc_context->out_buf_size, section_id);
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

    gettimeofday(&t2, NULL);
    timersub(&t2, &t1, &dt);
    long open_usec = dt.tv_usec + 1000000 *dt.tv_sec;
    socket_ctx->open_time = open_usec;
    socket_ctx->execution_time = 0;

    return acc_context->in_buf;
}

unsigned long fpga_acc_do_job (struct acc_context_t * acc_context, const char * param, unsigned int job_len, void ** result_buf){
    struct timeval t1, t2, dt;
    gettimeofday(&t1, NULL);
    struct socket_context_t * socket_ctx = (struct socket_context_t * ) (acc_context->socket_context);
    unsigned long result_buf_size;
    int status = atoi(socket_ctx->status);
    switch (status){
        case 0 :{//remote ACC is available;
            //printf("do REMOTE Job...\n");
            result_buf_size = remote_acc_do_job((void *)acc_context, param, job_len, result_buf); 
            break;
        }
        case 3 :{//local ACC is available;
            if (DEBUG)
                //printf("do LOCAL job...\n");
            result_buf_size = acc_do_job(&(acc_context->acc_handler), param, job_len, result_buf); 
            break;
}           
        case '?':
            break;
    }
    //printf("result buf size=%ld\n", result_buf_size);
    gettimeofday(&t2, NULL);
    timersub(&t2, &t1, &dt);
    long usec = dt.tv_usec + 1000000 *dt.tv_sec;
    
    socket_ctx->execution_time += usec;
    //printf("open, exe: %ld, %ld\n", socket_ctx->open_time, socket_ctx->execution_time);

    return result_buf_size;
}
 
void fpga_acc_close(struct acc_context_t * acc_context) {

    struct socket_context_t *socket_ctx = (struct socket_context_t *)acc_context->socket_context;
    struct timeval t1, t2, dt;
    gettimeofday(&t1, NULL);

    int status = atoi(socket_ctx->status); 
    switch (status){
        case 3:{
            printf("close LOCAL acc\n");
            acc_close(&(acc_context->acc_handler));
            break;
            }
        case 0:{
            printf("close REMOTE acc\n");
            disconnect_with_server((void *)acc_context);
            break;
            }
        case '?':{
            printf("Error: unknown status.\n");
            break;
            }
    }

    gettimeofday(&t2, NULL);
    timersub(&t2, &t1, &dt);
    long close_usec = dt.tv_usec + 1000000 *dt.tv_sec;
    
    socket_ctx->close_time = close_usec;
    socket_ctx->total_time = socket_ctx->open_time + socket_ctx->execution_time + socket_ctx->close_time;
    //printf("open, exe, close: %ld, %ld, %ld, %ld\n", socket_ctx->open_time, socket_ctx->execution_time, socket_ctx->close_time, socket_ctx->total_time);

    report_to_scheduler((void *)acc_context);
    free_memory((void *)acc_context);
    
}

int build_connection_to_scheduler(void *acc_ctx) { 
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;
    struct socket_context_t *socket_ctx = (struct socket_context_t *)acc_context->socket_context;
    struct hostent *hp;	/* host information */
	int sockoptval = 1;
    int client_fd;
    struct sockaddr_in *my_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    struct sockaddr_in *scheduler_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    TEST_NEG(client_fd);
	setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof(int));
    memset((char *)my_addr, 0, sizeof(struct sockaddr));
    my_addr->sin_family = AF_INET;
    my_addr->sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr->sin_port = htons(0);

    if(bind(client_fd, (struct sockaddr *)my_addr, sizeof(struct sockaddr)) < 0){
        perror("bind failed");
        close(client_fd);
        return -1;
    }

    //printf("port=%d, host=%s\n", socket_ctx->scheduler_port, socket_ctx->scheduler_host );
    memset((char *)scheduler_addr, 0, sizeof(struct sockaddr));
    scheduler_addr->sin_family = AF_INET;
    scheduler_addr->sin_port = htons(socket_ctx->scheduler_port);
    hp = gethostbyname(socket_ctx->scheduler_host);
    if (!hp) {
        printf("wrong host name\n");
        close(client_fd);
        return -1;
    }
    memcpy((void *) &(scheduler_addr->sin_addr), hp->h_addr_list[0], hp->h_length);

    if (connect(client_fd, (struct sockaddr *)scheduler_addr, sizeof(struct sockaddr)) <0){
        perror("connect failed");
        close(client_fd);
        return -1;
    }
    socket_ctx->to_scheduler_fd = client_fd;
    socket_ctx->to_scheduler_addr = (void *)my_addr; 
    socket_ctx->scheduler_addr = (void *)scheduler_addr;
    return client_fd;
}



int request_to_scheduler(void *acc_ctx) {
    int fd, status;
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;
    struct socket_context_t *socket_ctx = (struct socket_context_t *)acc_context->socket_context;

    unsigned int real_in_size = acc_context->real_in_buf_size;
    unsigned int in_buf_size = acc_context->in_buf_size;
    unsigned int out_buf_size = acc_context->out_buf_size;
    char *acc_name = acc_context->acc_name;

    struct client_to_scheduler send_ctx;
    struct scheduler_to_client recv_ctx;
    fd = socket_ctx->to_scheduler_fd;
    memset((void *)&send_ctx, 0, sizeof(send_ctx));
    memset((void *)&recv_ctx, 0, sizeof(recv_ctx));
    sprintf(send_ctx.real_in_size, "%u", real_in_size);
    sprintf(send_ctx.in_buf_size, "%u", in_buf_size);
    sprintf(send_ctx.out_buf_size, "%u", out_buf_size);
    memcpy(send_ctx.acc_name, acc_name, strlen(acc_name));

    memcpy(send_ctx.status, "open", strlen("open"));
    send(fd, (char *)&send_ctx, sizeof(send_ctx), 0);//send_request;
    recv(fd, (char *)&recv_ctx, sizeof(recv_ctx), 0);//recv_response 

    strcpy(socket_ctx->server_host, recv_ctx.host);
    strcpy(socket_ctx->job_id, recv_ctx.job_id);
    strcpy(socket_ctx->section_id, recv_ctx.section_id);
    strcpy(socket_ctx->status, recv_ctx.status);
    socket_ctx->server_port = atoi(recv_ctx.port);
    status = atoi(recv_ctx.status);
    close(fd);
   /* if (DEBUG){
        printf("socket_ctx->status:%s\n", socket_ctx->status);
        printf("status=%d\n", status);
    }
    */
    return status;
}


void report_to_scheduler(void *acc_ctx){

    struct debug_context_t debug_ctx;
    char response[16];
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;
    struct socket_context_t *socket_ctx = (struct socket_context_t *)acc_context->socket_context;
    struct sockaddr *scheduler_addr = (struct sockaddr *)(socket_ctx->scheduler_addr);
    struct sockaddr *my_addr = (struct sockaddr *)(socket_ctx->to_scheduler_addr);
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    int sockoptval = 1;
	setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof(int));
    if(bind(client_fd, my_addr, sizeof(struct sockaddr)) < 0){
        perror("bind failed");
        close(client_fd);
        return;
    }
    if (connect(client_fd, scheduler_addr, sizeof(struct sockaddr))< 0) {
        perror("connect failed\n");
        return;
    }

    memset(response,0, 16);

    memset((char *)&debug_ctx, 0, sizeof(debug_ctx));
    strcpy(debug_ctx.status,"close");
    strcpy(debug_ctx.job_id, socket_ctx->job_id);
    sprintf(debug_ctx.open_time, "%ld", socket_ctx->open_time);
    sprintf(debug_ctx.execution_time, "%ld", socket_ctx->execution_time);
    sprintf(debug_ctx.close_time, "%ld", socket_ctx->close_time);
    sprintf(debug_ctx.total_time, "%ld", socket_ctx->total_time);




    send(client_fd, (char *)&debug_ctx, sizeof(struct debug_context_t), 0);
    recv(client_fd, response, 16, 0);
    //printf("response from schduler: %s\n", response);
    return;
}

