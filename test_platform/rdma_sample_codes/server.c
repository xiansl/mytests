#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>

#define TEST_NZ(x) do { if ( (x)) die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) die("error: " #x " failed (returned zero/null)."); } while (0)

const int SEND_BUFFER_SIZE = 4*1024*1024;
const int RECV_BUFFER_SIZE = 4*1024*1024;

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

int on_connect_request(struct rdma_cm_id *id);
int on_connection(struct rdma_cm_id *id);
int on_disconnect(struct rdma_cm_id *id);
int on_event(struct rdma_cm_event *event);
void die(const char *reason);

void build_context(struct ibv_context *verbs);
void build_connection(struct rdma_cm_id *id);
void on_completion(struct ibv_wc *wc);

void build_qp_attr(struct ibv_qp_init_attr *qp_attr);
void * poll_cq(void *);
void register_memory(struct connection *conn);

void send_message(struct connection *conn);
void send_mr(void *context);

void post_receive(struct connection *con);
void post_receive_for_msg(struct connection *con);

void write_remote(struct connection * conn, uint32_t len);

struct context *s_ctx = NULL;


void die(const char *reason)
{
  fprintf(stderr, "%s\n", reason);
  exit(EXIT_FAILURE);
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

  TEST_NZ(pthread_create(&s_ctx->cq_poller_thread, NULL, poll_cq, NULL));
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

void * poll_cq(void *ctx)
{
  struct ibv_cq *cq;
  struct ibv_wc wc;

  while (1) {
    TEST_NZ(ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx));
    ibv_ack_cq_events(cq, 1);
    TEST_NZ(ibv_req_notify_cq(cq, 0));

    while (ibv_poll_cq(cq, 1, &wc))
      on_completion(&wc);
  }

  return NULL;
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

void register_memory(struct connection *conn)
{
  conn->send_region = malloc(SEND_BUFFER_SIZE);
  conn->recv_region = malloc(RECV_BUFFER_SIZE);

  conn->send_msg = malloc(sizeof(struct message));
  conn->recv_msg = malloc(sizeof(struct message));

  TEST_Z(conn->send_region_mr = ibv_reg_mr(
    s_ctx->pd, 
    conn->send_region, 
    SEND_BUFFER_SIZE, 
    IBV_ACCESS_LOCAL_WRITE));

  TEST_Z(conn->recv_region_mr = ibv_reg_mr(
    s_ctx->pd, 
    conn->recv_region, 
    RECV_BUFFER_SIZE, 
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
void build_connection(struct rdma_cm_id *id){

  struct ibv_qp_init_attr qp_attr;
  struct connection *conn;
  build_context(id->verbs);
  build_qp_attr(&qp_attr);

  TEST_NZ(rdma_create_qp(id, s_ctx->pd, &qp_attr));

  id->context = conn = (struct connection *)malloc(sizeof(struct connection));

  conn->id = id;
  conn->qp = id->qp;

  register_memory(conn);
  post_receive_for_msg(conn);

}

int on_connect_request(struct rdma_cm_id *id)
{
    printf("received connection request.\n");

    struct rdma_conn_param cm_params;
    printf("building connection......\n");
    build_connection(id);
    printf("building param......\n");
    build_params(&cm_params);
    //struct connection *conn = id->context;



  TEST_NZ(rdma_accept(id, &cm_params));
  printf("accept connection\n");

  return 0;
}
void server_pre_disconnect(struct connection *conn, uint32_t len){
    write_remote(conn,len);
}
void write_remote(struct connection * conn, uint32_t len){
    uint32_t size =len&(~(1U<<31));
    sprintf(conn->send_region, "message from passive/server side with pid %d", getpid());

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
void on_completion(struct ibv_wc *wc)
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
        send_mr(conn);
        printf("wait for remote write from client\n");
        post_receive(conn);
    }
    else if(wc->opcode == IBV_WC_RECV_RDMA_WITH_IMM){
        printf("received buffer from client:\n%s\n", conn->recv_region);
        uint32_t len = ntohl(wc->imm_data);
        uint32_t size = len &(~(1U<<31));
        uint32_t flag = len>>31;
        printf("recv_size=%u\n",size);
        if(flag ==1){
            server_pre_disconnect(conn, (uint32_t) 1024);
        }
        else {
            write_remote(conn, (uint32_t) 1024);
            post_receive(conn);
        } 
    }
}



int on_connection(struct rdma_cm_id *id)
{

  return 0;
}

int on_disconnect(struct rdma_cm_id *id)
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

  return 0; /* exit event loop */
}

int on_event(struct rdma_cm_event *event)
{
  int r = 0;

  if (event->event == RDMA_CM_EVENT_CONNECT_REQUEST)
    r = on_connect_request(event->id);
  else if (event->event == RDMA_CM_EVENT_ESTABLISHED){
      printf("event = RDMA_CM_EVENT_ESTABLISHED\n");
    r = on_connection(event->id->context);
  }
  else if (event->event == RDMA_CM_EVENT_DISCONNECTED){
    r = on_disconnect(event->id);
  }
  else
    die("on_event: unknown event.");

  return r;
}

int main(int argc, char **argv)
{
  struct sockaddr_in addr;
  struct rdma_cm_event *event = NULL;
  struct rdma_cm_id *listener = NULL;
  struct rdma_event_channel *ec = NULL;
  uint16_t port = 0;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;

  TEST_Z(ec = rdma_create_event_channel());
  TEST_NZ(rdma_create_id(ec, &listener, NULL, RDMA_PS_TCP));
  TEST_NZ(rdma_bind_addr(listener, (struct sockaddr *)&addr));
  TEST_NZ(rdma_listen(listener, 10)); /* backlog=10 is arbitrary */

  port = ntohs(rdma_get_src_port(listener));

  printf("listening on port %d.\n", port);

  while (rdma_get_cm_event(ec, &event) == 0) {
    struct rdma_cm_event event_copy;

    memcpy(&event_copy, event, sizeof(*event));
    rdma_ack_cm_event(event);

    if (on_event(&event_copy))
      break;
  }

  printf("ready to destroy event\n");
  rdma_destroy_event_channel(ec);

  return 0;
}
