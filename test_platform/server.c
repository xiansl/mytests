#define OS_LINUX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <memory.h>
#include "fpga-sim/driver/fpga-libacc.h" 

#define TEST_NZ(x) do { if ( (x)) die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) die("error: " #x " failed (returned zero/null)."); } while (0)

struct message
{
  union
  {
    struct {
        uint64_t addr;
        uint32_t rkey;
    }mr;
  } data;
};

struct context {
  struct ibv_context *ctx;
  struct ibv_pd *pd;
  struct ibv_cq *cq;
  struct ibv_comp_channel *comp_channel;
  pthread_t cq_poller_thread;
};

struct poll_cq_struct_t{
  void *ctx;
  struct rpc_param_t * rpc_param;
};

struct connection {
  struct rdma_cm_id *id;
  struct ibv_qp *qp;

  char *recv_region;
  char *send_region;

  struct message *recv_msg;
  struct message *send_msg;

  struct ibv_mr *recv_region_mr;
  struct ibv_mr *send_region_mr;

  struct ibv_mr *recv_msg_mr;
  struct ibv_mr *send_msg_mr;

  uint64_t peer_addr;
  uint32_t peer_rkey;

};

struct context *s_ctx = NULL;

char * get_server_ip_addr(void);
void die(const char *reason);
int on_connect_request(struct rdma_cm_id *id, struct rpc_param_t *rpc_param);
int on_connection(struct rdma_cm_id *id);
int on_disconnect(struct rdma_cm_id *id, struct rpc_param_t *rpc_param);
int on_setup_event(struct rdma_cm_event *event, struct rpc_param_t *rpc_param);

int on_disconnect_event(struct rdma_cm_event *event, struct rpc_param_t * rpc_param);

void build_context(struct ibv_context *verbs, struct rpc_param_t * rpc_param);
void build_connection(struct rdma_cm_id *id, struct rpc_param_t *rpc_param);
int on_completion(struct ibv_wc *wc, struct rpc_param_t * rpc_param);

void build_qp_attr(struct ibv_qp_init_attr *qp_attr);
void poll_cq(struct poll_cq_struct_t * poll_cq_struct);
void register_memory(struct connection *conn, struct rpc_param_t *rpc_param);

void send_message(struct connection *conn);
void send_mr(void *context);

void post_receive(struct connection *con);
void post_receive_for_msg(struct connection *con);

void server_write_remote(struct connection * conn, uint32_t len);
void server_pre_disconnect(struct rpc_param_t * rpc_param, struct connection *conn, uint32_t len);


int local_fpga_open (void * server_param);
void local_fpga_do_job (struct rpc_param_t * rpc_param);
void local_fpga_close (void *server_param);
void * rdma_server_data_transfer(void * server_param);

long do_my_work(struct acc_handler_t * acc_handler, char * param, void ** out_buffer) {

	long ret;
	unsigned int job_len;

//	Here you set a software-accelerator co-defined parameter

//	Here you copy your source data into this buffer
//	Copy "your data" into "acc_handler->data_buffer"
	memset (acc_handler->data_buffer, 0, sysconf(_SC_PAGESIZE));

//	Here you set the real data length in the source data buffer
//	so that accelerator know the real data length in the buffer
	job_len = 1024;

//	Here the software sends the job to accelerator and wait completion
	ret = acc_do_job (acc_handler, param, job_len, out_buffer);
	return ret; 
}

int local_fpga_open (void * server_param) {

    struct rpc_param_t * my_fpga_param = (struct rpc_param_t *)server_param;
    printf ("server_fpga_acc_name is %s\n", my_fpga_param->acc_name);
    int section_id = atoi(my_fpga_param->section_id);
    printf("section_id =%d\n", section_id); 
    printf("in_buf = %d\n", my_fpga_param->in_buf_size);

    //open accelerator
    my_fpga_param->in_buf =pri_acc_open(
        &(my_fpga_param->acc_handler),
        my_fpga_param->acc_name,
        my_fpga_param->in_buf_size,
        my_fpga_param->out_buf_size,
	section_id);

    if(my_fpga_param->in_buf == NULL) {
        printf("open %s fails.\n", my_fpga_param->acc_name);
        strcpy(my_fpga_param->status, "1");//set status to 1 means fail to open FPGA
        return atoi(my_fpga_param->status);
    }
    printf ("open %s successfully\n", my_fpga_param->acc_name);
    return atoi(my_fpga_param->status);
}

void local_fpga_do_job (struct rpc_param_t * rpc_param) {
    printf("do job.\n");
    //after copying data from rdma's recv_region to FPGA data_buffer
    char param[16];
    param[0] = 0x24;
    void *result_buf = NULL;
    long ret = acc_do_job (&(rpc_param->acc_handler), param, rpc_param->in_buf_size, &(result_buf));
    rpc_param->out_buf = result_buf;
    return; 
}

void local_fpga_close (void *server_param) {
    struct rpc_param_t * my_fpga_param = (struct rpc_param_t *)server_param;
    acc_close(&(my_fpga_param->acc_handler));
    printf("close acc successfully.\n");
    return;
}

void rdma_server_open(void * server_param) {

    	struct      server_param_t * my_param = (struct server_param_t *)server_param;
	int         rdma_port;
    	pthread_t   pthread;

	if (pipe(my_param->pipe) < 0) {
		printf ("Open pipe error\n");
        	strcpy(my_param->status,"2");
		return;
	}

    	//open rdma_server_data_transfer thread
    	if(pthread_create(&pthread, NULL, rdma_server_data_transfer, server_param)!=0) {
		fprintf(stderr, "Error creating thread\n");
        	strcpy(my_param->status,"2");//set status to 2 means fail to open RDMA listening thread.
	    	return;
    	}

	if(read(my_param->pipe[0], &rdma_port, sizeof(int)) == -1){
        	printf("Fail to read from pipe\n");
		strcpy(my_param->status,"2");
        	return;
    	}
	pthread_detach(pthread);
	sprintf (my_param->port, "%d", rdma_port);

    	char * rdma_ip_addr = get_server_ip_addr();
    	strcpy(my_param->ipaddr, rdma_ip_addr);

    	printf("Thread created successfully. port = %s\n", my_param->port);

    	return;
}

void * rdma_server_data_transfer(void * server_param)
{
    	struct      server_param_t * my_param = (struct server_param_t *)server_param;
	struct 	    rpc_param_t rpc_param_struct;
 	struct	    rpc_param_t * rpc_param = &(rpc_param_struct);
	strcpy(rpc_param->section_id, my_param->section_id);
	strcpy(rpc_param->status, my_param->status);
	strcpy(rpc_param->acc_name, my_param->acc_name);
	rpc_param->in_buf_size = my_param->in_buf_size;
	rpc_param->out_buf_size =  my_param->out_buf_size;

	int fpga_status = local_fpga_open(rpc_param);
	sprintf(my_param->status, "%d", fpga_status);
	if(fpga_status){ 
		printf("fail to open %s.\n", my_param->acc_name);
		return NULL;
	
	}
	
    	struct      sockaddr_in addr;
    	struct      rdma_cm_event *event = NULL;
    	struct      rdma_cm_id *listener = NULL;
    	struct      rdma_event_channel *ec = NULL;
    	uint16_t    port = 0;

    	memset(&addr, 0, sizeof(addr));
    	addr.sin_family = AF_INET;

    	TEST_Z(ec = rdma_create_event_channel());
    	TEST_NZ(rdma_create_id(ec, &listener, NULL, RDMA_PS_TCP));
    	TEST_NZ(rdma_bind_addr(listener, (struct sockaddr *)&addr));
    	TEST_NZ(rdma_listen(listener, 10)); /* backlog=10 is arbitrary */

    	port = ntohs(rdma_get_src_port(listener));

    	printf("listening on port %d.\n", port);
    	write (my_param->pipe[1], &port, sizeof(int));

    	while (rdma_get_cm_event(ec, &event) == 0) {
    	    struct rdma_cm_event event_copy;
    	    memcpy(&event_copy, event, sizeof(*event));
    	    rdma_ack_cm_event(event);
    	    if (on_setup_event(&event_copy, rpc_param))
    	        break;
    	}

	struct poll_cq_struct_t * poll_cq_struct = (struct poll_cq_struct_t *) malloc(sizeof(struct poll_cq_struct_t));
  	poll_cq_struct->ctx = NULL;
  	poll_cq_struct->rpc_param = rpc_param;
	
	poll_cq(poll_cq_struct);	

	
    	while (rdma_get_cm_event(ec, &event) == 0) {
    	    struct rdma_cm_event event_copy;
    	    memcpy(&event_copy, event, sizeof(*event));
    	    rdma_ack_cm_event(event);
    	    if (on_disconnect_event(&event_copy, rpc_param))
    	        break;
    	}

    	printf("disconnect...\n");
    	rdma_destroy_event_channel(ec);
	free(poll_cq_struct);

    	return NULL;
}
/*****************/

char * get_server_ip_addr(void){
	int fd;
	struct ifreq ifr;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, "ib0", IFNAMSIZ-1);
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);
	return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
} 

void die(const char *reason)
{
  fprintf(stderr, "%s\n", reason);
  exit(EXIT_FAILURE);
}

void build_context(struct ibv_context *verbs, struct rpc_param_t * rpc_param)
{
  //printf("line %d\n", __LINE__);
  if (s_ctx) {
    if (s_ctx->ctx != verbs)
      die("cannot handle events in more than one context.");

    return;
  }
  //struct poll_cq_struct_t * poll_cq_struct = (struct poll_cq_struct_t *)malloc(sizeof(struct poll_cq_struct_t));

  s_ctx = (struct context *)malloc(sizeof(struct context));

  s_ctx->ctx = verbs;

  TEST_Z(s_ctx->pd = ibv_alloc_pd(s_ctx->ctx));
  TEST_Z(s_ctx->comp_channel = ibv_create_comp_channel(s_ctx->ctx));
  TEST_Z(s_ctx->cq = ibv_create_cq(s_ctx->ctx, 10, NULL, s_ctx->comp_channel, 0)); /* cqe=10 is arbitrary */
  TEST_NZ(ibv_req_notify_cq(s_ctx->cq, 0));

  //printf("line %d\n", __LINE__);
  //TEST_NZ(pthread_create(&s_ctx->cq_poller_thread, NULL, poll_cq, (void *)poll_cq_struct));
  //printf("line %d\n", __LINE__);


}


void build_qp_attr(struct ibv_qp_init_attr *qp_attr)
{
  memset(qp_attr, 0, sizeof(*qp_attr));

  qp_attr->send_cq = s_ctx->cq;
  qp_attr->recv_cq = s_ctx->cq;
  qp_attr->qp_type = IBV_QPT_RC;

  qp_attr->cap.max_send_wr = 10;
  qp_attr->cap.max_recv_wr = 10;
  qp_attr->cap.max_send_sge = 1;
  qp_attr->cap.max_recv_sge = 1;
}

void poll_cq(struct poll_cq_struct_t * poll_cq_struct)
{
  struct ibv_cq *cq;
  struct ibv_wc wc;
  int ret;

  void *ctx = poll_cq_struct->ctx;
  struct rpc_param_t * rpc_param = (struct rpc_param_t *)poll_cq_struct->rpc_param;


  while (1) {
    TEST_NZ(ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx));
    ibv_ack_cq_events(cq, 1);
    TEST_NZ(ibv_req_notify_cq(cq, 0));

    while (ibv_poll_cq(cq, 1, &wc)){
      ret = on_completion(&wc, rpc_param);
    }
    if (ret)
      break;
  }
  return;
}

void post_receive_for_msg(struct connection *conn){
  struct ibv_recv_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = (uintptr_t)conn;
  wr.sg_list = &sge;
  wr.num_sge = 1;

  sge.addr = (uintptr_t)conn->recv_msg;
  sge.length = sizeof(*conn->recv_msg);
  sge.lkey = conn->recv_msg_mr->lkey;

  TEST_NZ(ibv_post_recv(conn->qp, &wr, &bad_wr));

}
void post_receive(struct connection *conn)
{
  struct ibv_recv_wr wr, *bad_wr = NULL;
  memset(&wr, 0, sizeof(wr));
  wr.wr_id = (uintptr_t)conn;
  wr.next = NULL;
  wr.num_sge = 0;

  TEST_NZ(ibv_post_recv(conn->qp, &wr, &bad_wr));
}

void register_memory(struct connection *conn, struct rpc_param_t *rpc_param)
{
    unsigned int recv_buffer_size = rpc_param->in_buf_size;
    unsigned int send_buffer_size = rpc_param->out_buf_size;
  conn->send_region = malloc(send_buffer_size);
  conn->recv_region = malloc(recv_buffer_size);

  rpc_param->in_buf = conn->recv_region;

  conn->send_msg = malloc(sizeof(struct message));
  conn->recv_msg = malloc(sizeof(struct message));

  TEST_Z(conn->send_region_mr = ibv_reg_mr(
    s_ctx->pd, 
    conn->send_region, 
    send_buffer_size, 
    IBV_ACCESS_LOCAL_WRITE));

  TEST_Z(conn->recv_region_mr = ibv_reg_mr(
    s_ctx->pd, 
    conn->recv_region, 
    recv_buffer_size, 
    IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

  TEST_Z(conn->send_msg_mr = ibv_reg_mr(
    s_ctx->pd, 
    conn->send_msg, 
    sizeof(struct message), 
    IBV_ACCESS_LOCAL_WRITE));

  TEST_Z(conn->recv_msg_mr = ibv_reg_mr(
    s_ctx->pd, 
    conn->recv_msg, 
    sizeof(struct message), 
    IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));
}

void send_message(struct connection *conn){

  struct ibv_send_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  printf("posting send message...\n");

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = (uintptr_t)conn;
  wr.opcode = IBV_WR_SEND;
  wr.sg_list = &sge;
  wr.num_sge = 1;
  wr.send_flags = IBV_SEND_SIGNALED;

  sge.addr = (uintptr_t)conn->send_msg;
  sge.length = sizeof(struct message);
  sge.lkey = conn->send_msg_mr->lkey;

  TEST_NZ(ibv_post_send(conn->qp, &wr, &bad_wr));
  return ;

}

void build_params(struct rdma_conn_param *params)
{
  memset(params, 0, sizeof(*params));

  params->initiator_depth = params->responder_resources = 1;
  params->rnr_retry_count = 7; /* infinite retry */
}

void send_mr(void *context){
    struct connection *conn = (struct connection *)context;
    conn->send_msg->data.mr.addr = (uintptr_t)conn->recv_region_mr->addr;
    conn->send_msg->data.mr.rkey = conn->recv_region_mr->rkey;

    send_message(conn);
}
void build_connection(struct rdma_cm_id *id, struct rpc_param_t *rpc_param){

  struct ibv_qp_init_attr qp_attr;
  struct connection *conn;
  build_context(id->verbs, rpc_param);
  build_qp_attr(&qp_attr);

  TEST_NZ(rdma_create_qp(id, s_ctx->pd, &qp_attr));

  id->context = conn = (struct connection *)malloc(sizeof(struct connection));

  conn->id = id;
  conn->qp = id->qp;

  register_memory(conn, rpc_param);
  post_receive_for_msg(conn);

}

int on_connect_request(struct rdma_cm_id *id, struct rpc_param_t *rpc_param)
{
    printf("received connection request.\n");

    struct rdma_conn_param cm_params;
    printf("building connection......\n");
    build_connection(id, rpc_param);
    printf("building param......\n");
    build_params(&cm_params);

    TEST_NZ(rdma_accept(id, &cm_params));
    printf("accept connection\n");

    return 0;
}


void server_pre_disconnect(struct rpc_param_t * rpc_param, struct connection *conn, uint32_t len){
    server_write_remote(conn,len);
    //local_fpga_close(rpc_param);
	
}
void server_write_remote(struct connection * conn, uint32_t len){
    uint32_t size =len&(~(1U<<31));
    //sprintf(conn->send_region, "message from passive/server side with pid %d", getpid());

    struct ibv_send_wr wr, *bad_wr = NULL; 
    struct ibv_sge sge;
 
    memset(&wr,0,sizeof(wr));
 
    wr.wr_id = (uintptr_t)conn;
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.send_flags = IBV_SEND_SIGNALED;
 
    wr.imm_data = htonl(len);
    wr.wr.rdma.remote_addr = (uintptr_t)conn->peer_addr;
    wr.wr.rdma.rkey = conn->peer_rkey;
 
    if (size >0) {
        wr.sg_list = &sge;
        wr.num_sge = 1;
    sge.addr = (uintptr_t)conn->send_region;
    sge.length = size;
    sge.lkey = conn->send_region_mr->lkey;
    }
    TEST_NZ(ibv_post_send(conn->qp, &wr, &bad_wr));
 
 
}
int on_completion(struct ibv_wc *wc, struct rpc_param_t * rpc_param)
{
  struct connection *conn = (struct connection *)(uintptr_t)wc->wr_id;

  if (wc->status != IBV_WC_SUCCESS)
    die("on_completion: status is not IBV_WC_SUCCESS.");

  if(wc->opcode == IBV_WC_SEND){
    printf("send completed successfully.\n");
  }
  else if (wc->opcode == IBV_WC_RECV){
        printf("received MR from client\n");

        conn->peer_addr = conn->recv_msg->data.mr.addr;
        conn->peer_rkey = conn->recv_msg->data.mr.rkey;
        post_receive(conn);
        send_mr(conn);
    }
    else if(wc->opcode == IBV_WC_RECV_RDMA_WITH_IMM){
        //printf("received buffer from client:\n%s\n", conn->recv_region);
        uint32_t len = ntohl(wc->imm_data);
        uint32_t size = len &(~(1U<<31));
        uint32_t flag = len>>31;
        printf("received %u bytes from client\n", size);
	

        if(flag ==1){
	printf("line %d\n",__LINE__);
            server_pre_disconnect(rpc_param, conn, (uint32_t) 0);
	printf("line %d\n",__LINE__);
	return 1;
        }

        else {
		//copy input data from RDMA to FPGA
        	memcpy(rpc_param->in_buf, conn->recv_region, rpc_param->in_buf_size);

        	//acceleration do job
        	local_fpga_do_job(rpc_param);

		//copy output data from FPGA to RDMA
		memcpy(conn->send_region, rpc_param->out_buf, rpc_param->out_buf_size);
            post_receive(conn);
            server_write_remote(conn, (uint32_t) rpc_param->out_buf_size);
        } 
    }
    return 0;
}



int on_connection(struct rdma_cm_id *id)
{
    return 1;
}

int on_disconnect(struct rdma_cm_id *id, struct rpc_param_t *rpc_param)
{
    struct connection *conn = (struct connection *)id->context;

    printf("disconnected.\n");

    rdma_destroy_qp(id);

    ibv_dereg_mr(conn->send_region_mr);
    ibv_dereg_mr(conn->recv_region_mr);

    ibv_dereg_mr(conn->send_msg_mr);
    ibv_dereg_mr(conn->recv_msg_mr);

    free(conn->send_region);
    free(conn->recv_region);

    free(conn->recv_msg);
    free(conn->send_msg);

    free(conn);

    rdma_destroy_id(id);
    //close local fpga device
    local_fpga_close(rpc_param);

  return 1; /* exit event loop */
}

int on_disconnect_event(struct rdma_cm_event *event, struct rpc_param_t * rpc_param)
{
  int r = 0;
  if (event->event == RDMA_CM_EVENT_DISCONNECTED){
    r = on_disconnect(event->id, rpc_param);
  }
  else
    die("on_event: unknown event.");

  return r;
}

int on_setup_event(struct rdma_cm_event *event, struct rpc_param_t *rpc_param)
{
  int r = 0;

  if (event->event == RDMA_CM_EVENT_CONNECT_REQUEST)
    r = on_connect_request(event->id, rpc_param);
  else if (event->event == RDMA_CM_EVENT_ESTABLISHED){
    r = on_connection(event->id->context);
  }
  //else if (event->event == RDMA_CM_EVENT_DISCONNECTED){
    //r = on_disconnect(event->id, rpc_param);
  //}
  else
    die("on_event: unknown event.");

  return r;
}


