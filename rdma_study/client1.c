#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <rdma/rdma_cma.h>
#include <sys/time.h>


#define TEST_NZ(x) do { if ( (x)) die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) die("error: " #x " failed (returned zero/null)."); } while (0)

unsigned int send_buffer_size, recv_buffer_size;
const int TIMEOUT_IN_MS = 500; /* ms */


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

  struct rdma_event_channel *ec;
  struct rdma_cm_id *id;

  pthread_t cq_poller_thread;
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

  enum {
    SS_INIT,
    SS_MR_SENT,
    SS_RDMA_SENT,
    SS_DONE_SENT
  } send_state;

  enum {
    RS_INIT,
    RS_MR_RECV,
    RS_DONE_RECV
  } recv_state;

};
struct context *s_ctx = NULL;


void die(const char *reason);

void register_memory(struct connection *conn);
void build_connection(struct rdma_cm_id *id);
void build_context(struct ibv_context *verbs);
void build_qp_attr(struct ibv_qp_init_attr *qp_attr);
void poll_cq(void *ctx);

int on_addr_resolved(struct rdma_cm_id *id);
int on_route_resolved(struct rdma_cm_id *id);
void on_connect(void *context);
void on_connection(struct rdma_cm_id *id);
void on_disconnect(struct rdma_cm_id *id);
int  on_event(struct rdma_cm_event *event);
void on_completion(struct ibv_wc *wc);

void send_message(struct connection *conn);
void send_mr(void *context);
void post_receive(struct connection *con);
void post_receive_for_msg(void *con);

void client_write(uint32_t len);
void client_write_once(uint32_t len);
void client_disconnect(void);
void client_remote_write(struct connection *conn, uint32_t n);

void client_test(char *ip, char *port) {


    struct addrinfo *addr;
    struct rdma_cm_event *event = NULL;
    struct rdma_cm_id *conn= NULL;
    struct rdma_event_channel *ec = NULL;
    struct timeval start, end, dt;
    struct timeval s, e, d;

    gettimeofday(&start, NULL);

    TEST_NZ(getaddrinfo(ip, port, NULL, &addr));
    TEST_Z(ec = rdma_create_event_channel());
    TEST_NZ(rdma_create_id(ec, &conn, NULL, RDMA_PS_TCP));
    TEST_NZ(rdma_resolve_addr(conn, NULL, addr->ai_addr, TIMEOUT_IN_MS));
    gettimeofday(&end, NULL);
    timersub(&end, &start, &dt);
    long usec = dt.tv_usec + 10000 * dt.tv_sec;
    freeaddrinfo(addr);
    printf("[Pre_Setup] takes %ld micro_secs.\n", usec);

    while (rdma_get_cm_event(ec, &event) == 0) {
        struct rdma_cm_event event_copy;

        memcpy(&event_copy, event, sizeof(*event));
        rdma_ack_cm_event(event);

        if (on_event(&event_copy)) {
            s_ctx->ec = ec;
            s_ctx->id = conn;

            gettimeofday(&s, NULL);

            on_connection(event_copy.id);//send our memory information to server using post_send
            poll_cq(NULL);//wait for send_completion
            poll_cq(NULL);//wait for recv_completion

            gettimeofday(&e, NULL);
            timersub(&e, &s, &d);
            usec = d.tv_usec + 10000 * d.tv_sec;
            printf("[RDMA Send & Recv] takes %ld micro_secs.\n", usec);
            break;
        }
    }
    return;

};

void client_disconnect(void){

    struct rdma_event_channel *ec = s_ctx->ec;
    struct rdma_cm_id *id = s_ctx->id;
    struct rdma_cm_event *event = NULL;
    struct timeval start, end, dt;

        gettimeofday(&start, NULL);
        printf("ready to disconnect\n");
        rdma_disconnect(id);
        printf("send disconnect\n");
        gettimeofday(&end, NULL);
        timersub(&end, &start, &dt); 
        long usec = dt.tv_usec + 1000000 * dt.tv_sec;
        printf("[rdma_disconnect] takes %ld micro_secs.\n", usec);

    while(1){
        if(rdma_get_cm_event(ec, &event) == 0) {
            struct rdma_cm_event event_copy;
            memcpy(&event_copy, event, sizeof(*event));
            rdma_ack_cm_event(event);
    
            if (event_copy.event == RDMA_CM_EVENT_DISCONNECTED){
                on_disconnect(event_copy.id);
                rdma_destroy_event_channel(ec);
                break;
            }
        }
    
    }

  return;

}

int main(int argc, char **argv)
{
    struct timeval start, end, dt;
	struct timeval t1, t2, dt1, dt2, dt3;

    if (argc != 4)
        die("usage: client <server-address> <server-port> <send_buffer_size>");
	int count = atoi(argv[3]);
	send_buffer_size = count * sysconf(_SC_PAGESIZE);
	recv_buffer_size = send_buffer_size;


        gettimeofday(&start, NULL);

        client_test(argv[1], argv[2]);//set up connection and exchange buffer_key and buffer_addr;
        gettimeofday(&t1, NULL);

        client_write_once((uint32_t) send_buffer_size);
        gettimeofday(&t2, NULL);

        client_disconnect();//disconnect and free locked memory;
        gettimeofday(&end, NULL);

        timersub(&end, &start, &dt); 

        timersub(&t1, &start, &dt1); 
        timersub(&t2, &t1, &dt2); 
        timersub(&end, &t2, &dt3); 

        long job_usec = dt.tv_usec + 1000000 * dt.tv_sec;

		printf("dt1 = %ld\n", dt1.tv_usec+1000000*dt1.tv_sec);
		printf("dt2 = %ld\n", dt2.tv_usec+1000000*dt2.tv_sec);
		printf("dt3 = %ld\n", dt3.tv_usec+1000000*dt3.tv_sec);
        printf("time used for a whole transfer job_usec=%ld\n", job_usec);

    return 0;
}

void die(const char *reason)
{
  fprintf(stderr, "%s\n", reason);
  exit(EXIT_FAILURE);
}

int on_event(struct rdma_cm_event *event)
{
  int r = 0;

  if (event->event == RDMA_CM_EVENT_ADDR_RESOLVED)
    r = on_addr_resolved(event->id);
  else if (event->event == RDMA_CM_EVENT_ROUTE_RESOLVED)
    r = on_route_resolved(event->id);
  else if (event->event == RDMA_CM_EVENT_ESTABLISHED)
      r = 1;
  else
    die("on_event: unknown event.");

  return r;
}


void build_context(struct ibv_context *verbs)
{
  if (s_ctx) {
    if (s_ctx->ctx != verbs)
      die("cannot handle events in more than one context.");

    return;
  }

  s_ctx = (struct context *)malloc(sizeof(struct context));

  s_ctx->ctx = verbs;

  TEST_Z(s_ctx->pd = ibv_alloc_pd(s_ctx->ctx));
  TEST_Z(s_ctx->comp_channel = ibv_create_comp_channel(s_ctx->ctx));
  TEST_Z(s_ctx->cq = ibv_create_cq(s_ctx->ctx, 10, NULL, s_ctx->comp_channel, 0)); /* cqe=10 is arbitrary */
  TEST_NZ(ibv_req_notify_cq(s_ctx->cq, 0));


  //TEST_NZ(pthread_create(&s_ctx->cq_poller_thread, NULL, poll_cq, NULL));
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

void  poll_cq(void *ctx)
{
  struct ibv_cq *cq;
  struct ibv_wc wc;
  int ne;

    TEST_NZ(ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx));//block by default
    ibv_ack_cq_events(cq, 1);
    TEST_NZ(ibv_req_notify_cq(cq, 0));

    do {
        ne = ibv_poll_cq(cq, 1, &wc);
        if(ne < 0){
            printf("fail to poll completion from the CQ. ret = %d\n", ne);
            return;
        }
        else if(ne == 0)
            continue;
        else
            on_completion(&wc);
    } while (ne == 0);

  return;
}

void post_receive_for_msg(void *con){
    struct connection *conn = (struct connection *)con;
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

void register_memory(struct connection *conn)
{
  conn->send_region = malloc(send_buffer_size);
  conn->recv_region = malloc(recv_buffer_size);
  memset(conn->recv_region,0, recv_buffer_size);

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

  //printf("posting send message...\n");

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
void build_connection(struct rdma_cm_id *id){

  struct ibv_qp_init_attr qp_attr;
  struct connection *conn;
  build_qp_attr(&qp_attr);

  TEST_NZ(rdma_create_qp(id, s_ctx->pd, &qp_attr));

  id->context = conn = (struct connection *)malloc(sizeof(struct connection));

  conn->id = id;
  conn->qp = id->qp;

}

int on_addr_resolved(struct rdma_cm_id *id)
{

    printf("address resolved.\n");
    build_context(id->verbs);
    build_connection(id);
    TEST_NZ(rdma_resolve_route(id, TIMEOUT_IN_MS));

  return 0;
}

void write_remote(struct connection * conn, uint32_t len){

    uint32_t size =len&(~(1U<<31));
    snprintf(conn->send_region, send_buffer_size, "message from active/client side with pid %d", getpid());
    struct ibv_send_wr wr, *bad_wr = NULL; 
    struct ibv_sge sge;

    memset(&wr,0,sizeof(wr));

    wr.wr_id = (uintptr_t)conn;
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.send_flags = IBV_SEND_SIGNALED;

    wr.imm_data = htonl(len);
    wr.wr.rdma.remote_addr = (uintptr_t)conn->peer_addr;
    wr.wr.rdma.rkey = conn->peer_rkey;

    if (size>0){
        wr.sg_list = &sge;
        wr.num_sge = 1;
    sge.addr = (uintptr_t)conn->send_region;
    sge.length = size;
    sge.lkey = conn->send_region_mr->lkey;
    }
    TEST_NZ(ibv_post_send(conn->qp, &wr, &bad_wr));


}

void on_completion(struct ibv_wc *wc)
{
    struct connection *conn = (struct connection *)(uintptr_t)wc->wr_id;

    if (wc->status !=IBV_WC_SUCCESS)
        die("on_completion: status is not ibv_wc_success.");

    if(wc->opcode == IBV_WC_SEND){
        printf("send completed successfully\n");
    }

    if (wc->opcode == IBV_WC_RECV){
        printf("received MR from server\n");
        conn->peer_addr = conn->recv_msg->data.mr.addr;
        conn->peer_rkey = conn->recv_msg->data.mr.rkey;
    }
    else if(wc->opcode == IBV_WC_RECV_RDMA_WITH_IMM){
        printf("received remote write from server\n%s\n",conn->recv_region);
        uint32_t len = ntohl(wc->imm_data);
        uint32_t size = len&(~(1U<<31));
        //uint32_t flag = len>>31;
        printf("recv_size =%u\n",size);
	}
    //else if (wc->opcode == IBV_WC_RDMA_WRITE){
    //    printf("not sure what opcode it should be, maybe this one means write to remote is posted successfully?\n");
    //}

}


void client_write(uint32_t len){
    struct timeval start, end, dt;
    gettimeofday(&start, NULL);
    struct connection *conn = (struct connection *)s_ctx->id->context;
    post_receive(conn);
    //uint32_t size = len|(1UL<<31);
    write_remote(conn,len);
    poll_cq(NULL);//wait for write completion
    poll_cq(NULL);//wait for recv completion
    gettimeofday(&end, NULL);
    timersub(&end, &start, &dt);
    long usec = dt.tv_usec + 1000000 * dt.tv_sec;
    printf("[Wriet] takes %ld micro_secs.\n", usec);

}

void client_write_once(uint32_t len){
    struct timeval start, end, dt;
    gettimeofday(&start, NULL);
    struct connection *conn = (struct connection *)s_ctx->id->context;
    post_receive(conn);
    uint32_t size = len|(1UL<<31);
    write_remote(conn,size);
    poll_cq(NULL);//wait for write completion
    poll_cq(NULL);//wait for recv completion
    gettimeofday(&end, NULL);
    timersub(&end, &start, &dt);
    long usec = dt.tv_usec + 1000000 * dt.tv_sec;
    printf("[Wriet] takes %ld micro_secs.\n", usec);
}

void send_mr(void *context)
{
    struct connection *conn = (struct connection *)context;
    conn->send_msg->data.mr.addr = (uintptr_t)conn->recv_region_mr->addr;
    conn->send_msg->data.mr.rkey = conn->recv_region_mr->rkey;

    send_message(conn);
}

void on_connection(struct rdma_cm_id *id)
{
    send_mr(id->context);
    return;//exit while loop
}


void on_disconnect(struct rdma_cm_id *id)
{
  struct timeval start, end, dt;
  gettimeofday(&start, NULL);
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

  rdma_destroy_id(id);

  free(conn);
  gettimeofday(&end, NULL);
  timersub(&end, &start, &dt);
  long usec = dt.tv_usec + 1000000 * dt.tv_sec;
  printf("[Derigester] takes %ld micro_secs.\n", usec);


  return; /* exit event loop */
}


void build_params(struct rdma_conn_param *params)
{
  memset(params, 0, sizeof(*params));

  params->initiator_depth = params->responder_resources = 1;
  params->rnr_retry_count = 7; /* infinite retry */
}

int on_route_resolved(struct rdma_cm_id *id)
{
  struct rdma_conn_param cm_params;
  struct timeval start, end, dt;

  printf("route resolved.\n");
  build_params(&cm_params);
  TEST_NZ(rdma_connect(id, &cm_params));

  gettimeofday(&start, NULL);
  register_memory((struct connection *)(id->context)); 
  gettimeofday(&end, NULL);
  timersub(&end, &start, &dt);
  long usec = dt.tv_usec + 1000000 * dt.tv_sec;
  printf("[Register] takes %ld micro_secs.\n", usec);

  post_receive_for_msg(id->context);

  return 0;
}
