#include "acc_servicelayer.h"
#include "tcp_transfer.h"

//size_t send_msg(int fd, void *buffer, size_t len, size_t chunk)
//{
//    char *c_ptr = (char *) buffer;
//    size_t block = chunk, left_block = len, sent_block = 0;
//    char flag[1];
//
//    if (len < chunk){ 
//        printf("send small message ........\n");
//        return send(fd, buffer, len, 0);
//    }
//
//    while(sent_block < len) {
//        printf("send.....\n");
//        block = send(fd, c_ptr, chunk, 0);
//        if (block < 0)
//            return block;
//        sent_block += block;
//        left_block -= block;
//        chunk = left_block > chunk ? chunk : left_block;
//        c_ptr += block;
//    }
//    send(fd, flag, 1,0);
//    return sent_block; 
//}
//    
//size_t recv_msg(int fd, void *buffer, size_t len, size_t chunk)
//{
//    char *c_ptr = buffer;
//    size_t block = 0, left_block = len, recv_block = 0;
//    char flag[1];
//
//    if (len < chunk){ 
//        printf("recv small message ........\n");
//        return recv(fd, buffer, len, 0);
//    }
//
//    while (recv_block < len) {
//        printf("recv.....\n");
//        block = recv(fd, c_ptr, chunk, 0);
//        if (block <= 1)
//            break;
//        c_ptr += block;
//        recv_block += block;
//        left_block -= chunk;
//        chunk = left_block > chunk ? chunk : left_block;
//    }
//    if (block < 0)
//        return block;
//    if (block > 1)
//        recv(fd, flag, 1, 0);
//    return recv_block; 
//}



size_t send_msg(int fd, void *buffer, size_t len, size_t chunk)
{
    char *c_ptr = (char *) buffer;
    size_t block = chunk, left_block = len, sent_block = 0;

    if (len < chunk){ 
        printf("send small message ........\n");
        return send(fd, buffer, len, 0);
    }

    while(sent_block < len) {
        printf("send.....\n");
        block = send(fd, c_ptr, chunk, 0);
        if (block <= 0){
            printf("nothing to send\n");
            break;
        }
        sent_block += block;
        left_block -= block;
        chunk = left_block > chunk ? chunk : left_block;
        c_ptr += block;
    }
    return sent_block; 
}
    
size_t recv_msg(int fd, void *buffer, size_t len, size_t chunk)
{
    char *c_ptr = buffer;
    size_t block = chunk, left_block = len, recv_block = 0;

    if (len < chunk){ 
        printf("recv small message ........\n");
        return recv(fd, buffer, len, 0);
    }

    while (recv_block < len) {
        printf("recv.....\n");
        block = recv(fd, c_ptr, chunk, 0);
        if (block <= 0){
            printf("nothing to recv\n");
            break;
        }
        recv_block += block;
        left_block -= block;
        chunk = left_block > chunk ? chunk : left_block;
        c_ptr += block;
    }
    return recv_block; 
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

    printf("port=%d, host=%s\n", socket_ctx->scheduler_port, socket_ctx->scheduler_host );
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

int build_connection_to_server(void *acc_ctx) {
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;
    struct socket_context_t *socket_ctx = (struct socket_context_t *)acc_context->socket_context;
    struct hostent *hp;	/* host information */
	int sockoptval = 1;
    int client_fd;
    unsigned int in_buf_size = acc_context->in_buf_size;
    unsigned int out_buf_size = acc_context->out_buf_size;
    //char *host = socket_ctx->server_host;
    int port = socket_ctx->server_port;
    struct sockaddr_in *my_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    struct sockaddr_in *server_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
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

    memset((char *)server_addr, 0, sizeof(struct sockaddr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(port);
    hp = gethostbyname("localhost");
    if (!hp) {
        close(client_fd);
        return -1;
    }
    memcpy((void *)&(server_addr->sin_addr), hp->h_addr_list[0], hp->h_length);


    if (connect(client_fd, (struct sockaddr *)server_addr, sizeof(struct sockaddr)) <0){
        perror("connect failed");
        close(client_fd);
        return -1;
    }
    socket_ctx->to_server_addr = (void *)my_addr;
    socket_ctx->server_addr = (void *)server_addr;
    socket_ctx->to_server_fd = client_fd;
    socket_ctx->in_buf = malloc(sizeof(in_buf_size));
    socket_ctx->out_buf = malloc(sizeof(out_buf_size));
    acc_context->in_buf = socket_ctx->in_buf;
    printf("open fd:%d\n", client_fd);
    return client_fd;
}


void build_context(void *acc_ctx, char *acc_name, unsigned int in_buf_size, unsigned int out_buf_size, const char *scheduler_host, int scheduler_port){
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;
    strcpy(acc_context->acc_name, acc_name);
    acc_context->in_buf_size = MIN(in_buf_size, MAX_BUFFER_SIZE);
    acc_context->out_buf_size = MIN(out_buf_size, MAX_BUFFER_SIZE);
    acc_context->real_in_buf_size = in_buf_size;
    acc_context->real_out_buf_size = out_buf_size;

    acc_context->socket_context = malloc(sizeof(struct socket_context_t));
    memset((char *)(acc_context->socket_context), 0, sizeof(struct socket_context_t));
    struct socket_context_t * socket_ctx = (struct socket_context_t *) (acc_context->socket_context);
    strcpy(socket_ctx->scheduler_host, scheduler_host);
    socket_ctx->scheduler_port = scheduler_port;
    return;
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
    if (DEBUG)
        printf("socket_ctx->status:%s\n", socket_ctx->status);

    status = atoi(recv_ctx.status);
    if (DEBUG)
        printf("status=%d\n", status);
    return status;
}

unsigned int remote_acc_do_job(void *acc_ctx, const char *param, unsigned int job_len, void ** result_buf) {
    struct acc_context_t *acc_context = (struct acc_context_t *) acc_ctx;
    struct socket_context_t *socket_ctx = (struct socket_context_t *)acc_context->socket_context;
    unsigned int recv_buf_size;
    *result_buf = socket_ctx->out_buf; 
    char *in_buf = (char *)socket_ctx->in_buf;
    int fd = socket_ctx->to_server_fd;
    printf("job_len = %d\n", job_len);
    size_t len = send_msg(fd, in_buf, job_len, MTU); 
    printf("send len = %zu !!!!!!!!!!!!!!!!!!!!!!\n", len);
    printf("LINE %d\n", __LINE__);
    printf("send to remote server...\n");
    recv_buf_size = recv_msg(fd, *result_buf, job_len, MTU);

    printf("recv buf size = %u\n", recv_buf_size);

    return recv_buf_size; 
} 

void free_memory(void *acc_ctx){
    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;
    struct socket_context_t *socket_ctx = (struct socket_context_t *)(acc_context->socket_context);
    free(socket_ctx->scheduler_addr);
    free(socket_ctx->server_addr);
    free(socket_ctx->to_scheduler_addr);
    free(socket_ctx->to_server_addr);

}

void socket_server_open(void *server_param){
    struct server_param_t * my_param = (struct server_param_t *)server_param;
    pthread_t server_thread;
    int thread_status;
    if (pipe(my_param->pipe) < 0) {
        printf("Open pipe error.\n");
        strcpy(my_param->status, "2");
        return;
    }
    if(pthread_create(&server_thread, NULL, tcp_server_data_transfer, my_param)!=0) {
        fprintf(stderr, "Error creating thread\n");
        strcpy(my_param->status,"2");
        return;
    }
    if (read(my_param->pipe[0], &thread_status, sizeof(int))== -1){
        printf("Fail to read status from thread\n");
        strcpy(my_param->status,"2");
        return;
    }
    pthread_detach(server_thread);
    sprintf(my_param->status, "%d", thread_status);
    if (DEBUG){
        printf("status from server:%s\n", my_param->status);
        printf("server listening to %s, port %d\n", my_param->ipaddr, (atoi)(my_param->port));
        printf("thread created successfully.\n");
    }
    return;

}

void * tcp_server_data_transfer(void * server_param) {

    struct server_param_t * my_param = (struct server_param_t *) server_param;
    struct server_context_t server_context;
    struct acc_handler_t acc_handler;
    memset((void *)&server_context,0,sizeof(server_context));
    server_context.acc_handler = &(acc_handler);
    strcpy(server_context.section_id, my_param->section_id);
    strcpy(server_context.acc_name, my_param->acc_name);
    server_context.in_buf_size = my_param->in_buf_size;
    server_context.out_buf_size = my_param->out_buf_size;
    printf("my_param->in_buf_size=%u, out_buf_size=%u\n",my_param->in_buf_size, my_param->out_buf_size);


    int status = local_fpga_open((void *)&server_context);
    printf("LINE %d\n.......", __LINE__);
    sprintf(my_param->status, "%d", status);
    if (status) {
        printf("Fail to open %s.\n", my_param->acc_name);
        return NULL;
    }

    /*open a server socket */
    struct sockaddr_in server_addr, client_addr;
    memset((void *)&server_addr, 0, sizeof(server_addr));
    memset((void *)&client_addr, 0, sizeof(client_addr));
    int server_fd;//listening socket providing service
    int rqst_fd;//socket accepting the request;
    int sockoptval = 1;
    char server_host[16];
    unsigned short server_port;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof(int));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(0);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        printf("bind failed\n");
        sprintf(my_param->status, "%d", 2);
        exit(1);
    }
    /*set up the socket for listening with a queue length of BACKLOG */

    if(listen(server_fd, BACKLOG) < 0) {
        printf("listen failed.\n");
        sprintf(my_param->status, "%d", 2);
        exit(1);
    }

	socklen_t alen = sizeof(server_addr);     /* length of address */
    getsockname(server_fd, (struct sockaddr *)&server_addr, &alen);
    server_port = ntohs(server_addr.sin_port);
    strcpy(server_host, inet_ntoa(server_addr.sin_addr));
    printf(" port is ........%d \n", server_port);
    sprintf(my_param->port, "%d", server_port);
    write(my_param->pipe[1], &status, sizeof(int));


    if ((rqst_fd = accept(server_fd, (struct sockaddr *)&client_addr, &alen)) <0 ) {
        printf("accept failed\n");
        exit (1);
    }
    printf("received connection from: %s, port: %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    size_t recv_len = (size_t) server_context.in_buf_size;
    size_t send_len = (size_t) server_context.out_buf_size;
    size_t recv_size, send_size;
    printf("recv_len = %zu, send_len=%zu\n", recv_len, send_len);
    printf("server_context->in_buf_size=%u, out_buf_size=%u\n",server_context.in_buf_size, server_context.out_buf_size);

    char *send_buff = (char *)memset(malloc(send_len),0, send_len); //buffer
    char *recv_buff = (char *)memset(malloc(recv_len),0, recv_len); //buffer

    while (1) {
    
        if( (recv_size = recv_msg(rqst_fd, recv_buff, recv_len, MTU)) < 0){
            printf("fail to receive\n");
            exit(1);
        }
        if (recv_size == 0){
            printf("disconnect\n");
            break;
        }
        /*do jo*/ 
        memcpy(server_context.in_buf, recv_buff, recv_len);
        unsigned long ret = local_fpga_do_job((void *)&server_context);
        printf("ret = %lu\n", ret);
        memcpy(send_buff, server_context.out_buf, send_len);
        
        if ( (send_size = send_msg(rqst_fd, send_buff, send_len, MTU))< 0){
            printf("fail to send\n");
            exit(1);
        }
        printf("send size from server:%zu\n", send_size);
    }
    close(rqst_fd);
    close(server_fd);
    local_fpga_close((void *)&server_context);
    return NULL;
}

int local_fpga_open(void * server_context){
    struct server_context_t * my_context = (struct server_context_t *)server_context;
    int section_id = atoi(my_context->section_id);
    my_context->in_buf = pri_acc_open((struct acc_handler_t *)(my_context->acc_handler), my_context->acc_name, my_context->in_buf_size, my_context->out_buf_size, section_id);
    if(my_context->in_buf == NULL){
        printf("open %s fails.\n", my_context->acc_name);
        return 1;
    }
    printf("open %s successfully\n", my_context->acc_name);

    return 0;//return status;
}
unsigned long local_fpga_do_job(void * server_context){
    struct server_context_t * my_context = (struct server_context_t *)server_context;
    char param[16];
    param[0]=0x24;
    void *result_buf = NULL;
    long ret = acc_do_job((struct acc_handler_t *)(my_context->acc_handler), param, my_context->in_buf_size, &(result_buf));
    my_context->out_buf = result_buf;
    printf("do job finished\n");
    return ret;
}

void local_fpga_close(void * server_context){
    struct server_context_t * my_context = (struct server_context_t *)server_context;
    acc_close((struct acc_handler_t *)(my_context->acc_handler));
    return;
}
    
