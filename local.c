/*
 * build:
 *   cc -o client client.c -lrdmacm
 *
 * usage:
 *   client <servername> <val1> <val2>
 *
 * connects to server, sends val1 via RDMA write and val2 via send,
 * and receives val1+val2 back from the server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

#include <infiniband/arch.h>
#include <rdma/rdma_cma.h>

#define VERBOSE
#ifdef VERBOSE 
#define DPRINTF(fmt, ...) \
    do { printf("local: " fmt, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
    do { } while (0)
#endif


int main(int argc, char *argv[])
{
	struct ibv_pd		       *pd1, *pd2;
	struct ibv_comp_channel	       *comp_chan1, *comp_chan2;
	struct ibv_cq		       *cq1, *cq2;
	struct ibv_cq		       *evt_cq = NULL;
	struct ibv_mr		       *mr1, *mr2;
	struct ibv_qp_init_attr		qp_attr1 = { }, qp_attr2 = {};
	struct ibv_sge			sge;
	struct ibv_send_wr		send_wr = { };
	struct ibv_send_wr	       *bad_send_wr = NULL;
	struct ibv_wc			wc;
	struct ibv_qp			*qp1, *qp2;
	void			       *cq_context = NULL;
	union ibv_gid			gid1, gid2;

	int				n;

	uint8_t			       *buf1, *buf2;

	int				err;
	int 				num_devices;
	struct ibv_context	*	verbs1, *verbs2;
	struct ibv_device ** dev_list = ibv_get_device_list(&num_devices);
	struct ibv_device_attr		dev_attr;
	int use = 0;
	int port = 1;
	int x = 0;
	unsigned long mb = 0;
	unsigned long bytes = 0;
	unsigned long save_diff = 0;
	struct timeval start, stop, diff;
	int iterations = 0;

	struct rusage usage;
	struct timeval ustart, uend;
	struct timeval sstart, send;
	struct timeval tstart, tend;

	DPRINTF("There are %d devices\n", num_devices);

	for(x = 0; x < num_devices; x++) {
		printf("Device: %d, %s\n", x, ibv_get_device_name(dev_list[use]));
	}

	if(num_devices == 0 || dev_list == NULL) {
		printf("No devices found\n");
		return 1;
	}

	if(argc < 2) {
		printf("Which RDMA device to use? 0, 1, 2, 3...\n");
		return 1;
	}
	
	use = atoi(argv[1]);

	DPRINTF("Using device %d\n", use);

	verbs1 = ibv_open_device(dev_list[use]);

	if(verbs1 == NULL) {
		printf("Failed to open device!\n");
		return 1;
	}

	DPRINTF("Device open %s\n", ibv_get_device_name(dev_list[use]));

	verbs2 = ibv_open_device(dev_list[use]);

	if(verbs2 == NULL) {
		printf("Failed to open device again!\n");
		return 1;
	}

	if(ibv_query_device(verbs1, &dev_attr)) {
		printf("Failed to query device attributes.\n");
		return 1;
	}
		
	printf("Device open: %d, %s which has %d ports\n", x, ibv_get_device_name(dev_list[use]), dev_attr.phys_port_cnt);

	if(argc < 3) {
		printf("Which port on the device to use? 1, 2, 3...\n");
		return 1;
	}

	port = atoi(argv[2]);
	
	if(port <= 0) {
		printf("Port #%d invalid, must start with 1, 2, 3, ...\n", port);
		return 1;
	}

	printf("Using port %d\n", port);
	
	if(argc < 4) {
		printf("How many iterations to perform?\n");
		return 1;
	}

	iterations = atoi(argv[3]);
	printf("Will perform %d iterations\n", iterations);

	pd1 = ibv_alloc_pd(verbs1);
	if (!pd1)
		return 1;

	if(argc < 5) {
		printf("How many megabytes to allocate? (This will be allocated twice. Once for source, once for destination.)\n");
		return 1;
	}

	mb = atoi(argv[4]);
	
	if(mb <= 0) {
		printf("Megabytes %lu invalid\n", mb);
		return 1;
	}

	DPRINTF("protection domain1 allocated\n");

	pd2 = ibv_alloc_pd(verbs2);
	if (!pd2)
		return 1;

	DPRINTF("protection domain2 allocated\n");

	comp_chan1 = ibv_create_comp_channel(verbs1);
	if (!comp_chan1)
		return 1;

	DPRINTF("completion chan1 created\n");

	comp_chan2 = ibv_create_comp_channel(verbs2);
	if (!comp_chan2)
		return 1;

	DPRINTF("completion chan2 created\n");

	cq1 = ibv_create_cq(verbs1, 2, NULL, comp_chan1, 0);
	if (!cq1)
		return 1;

	DPRINTF("CQ1 created\n");

	cq2 = ibv_create_cq(verbs2, 2, NULL, comp_chan2, 0);
	if (!cq2)
		return 1;

	DPRINTF("CQ2 created\n");

	bytes = mb * 1024UL * 1024UL;

	buf1 = malloc(bytes);
	if (!buf1)
		return 1;

	buf2 = malloc(bytes);
	if (!buf2)
		return 1;

	printf("Populating %lu MB memory.\n", mb * 2);

	for(x = 0; x < bytes; x++) {
		buf1[x] = 123;
	}

	buf1[bytes - 1] = 123;

	mr1 = ibv_reg_mr(pd1, buf1, bytes, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
	if (!mr1) {
        printf("Failed to register memory.\n");
		return 1;
    }

	mr2 = ibv_reg_mr(pd2, buf2, bytes, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
	if (!mr2) {
        printf("Failed to register memory.\n");
		return 1;
        }

	DPRINTF("memory registered.\n");

	qp_attr1.cap.max_send_wr	 = 10;
	qp_attr1.cap.max_send_sge = 10;
	qp_attr1.cap.max_recv_wr	 = 10;
	qp_attr1.cap.max_recv_sge = 10;
	qp_attr1.sq_sig_all = 1;

	qp_attr1.send_cq		 = cq1;
	qp_attr1.recv_cq		 = cq1;

	qp_attr1.qp_type		 = IBV_QPT_RC;

	qp1 = ibv_create_qp(pd1, &qp_attr1);
	if (!qp1) {
		printf("failed to create queue pair #1\n");
		return 1;
	}

	DPRINTF("queue pair1 created\n");

	qp_attr2.cap.max_send_wr	 = 10;
	qp_attr2.cap.max_send_sge = 10;
	qp_attr2.cap.max_recv_wr	 = 10;
	qp_attr2.cap.max_recv_sge = 10;
	qp_attr2.sq_sig_all = 1;

	qp_attr2.send_cq		 = cq2;
	qp_attr2.recv_cq		 = cq2;

	qp_attr2.qp_type		 = IBV_QPT_RC;


	qp2 = ibv_create_qp(pd2, &qp_attr2);
	if (!qp2) {
		printf("failed to create queue pair #2\n");
		return 1;
	}

	DPRINTF("queue pair2 created\n");

	struct ibv_qp_attr attr1 = {
		.qp_state = IBV_QPS_INIT,
		.pkey_index = 0,
		.port_num = port,
		.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE,
	};

	if(ibv_modify_qp(qp1, &attr1,
		IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS)) {
		printf("verbs 1 Failed to go to init\n");
		return 1;
	}	

	DPRINTF("verbs1 to init\n");

	struct ibv_qp_attr attr2 = {
		.qp_state = IBV_QPS_INIT,
		.pkey_index = 0,
		.port_num = port,
		.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE,
	};

	if(ibv_modify_qp(qp2, &attr2,
		IBV_QP_STATE |
		IBV_QP_PKEY_INDEX |
		IBV_QP_PORT |
		IBV_QP_ACCESS_FLAGS)) {
		printf("verbs 2 Failed to go to init\n");
		return 1;
	}

	DPRINTF("verbs2 to init\n");

	//struct ibv_gid gid1, gid2;
	struct ibv_port_attr port1, port2;
	uint64_t psn1 = lrand48() & 0xffffff;
	uint64_t psn2 = lrand48() & 0xffffff;

	if(ibv_query_port(verbs1, port, &port1))
		return 1;
	
	DPRINTF("got port1 information\n");

	if(ibv_query_port(verbs2, port, &port2))
		return 1;
	
	DPRINTF("got port2 information\n");

	if(ibv_query_gid(verbs1, 1, 0, &gid1))
		return 1;
	DPRINTF("got gid1 information\n");

	if(ibv_query_gid(verbs2, 1, 0, &gid2))
		return 1;

	DPRINTF("got gid2 information\n");

	struct ibv_qp_attr next2 = {
			.qp_state = IBV_QPS_RTR,
			.path_mtu = IBV_MTU_1024,
			.dest_qp_num = qp2->qp_num,
			.rq_psn = psn2,
			.max_dest_rd_atomic = 5,
			.min_rnr_timer = 12,
			.ah_attr = {
				.is_global = 0,
				.dlid = port2.lid,
				.sl = 0,
				.src_path_bits = 0,
				.port_num = port,
			}
	};

	if(gid2.global.interface_id) {
		next2.ah_attr.is_global = 1;
		next2.ah_attr.grh.hop_limit = 1;
		next2.ah_attr.grh.dgid = gid2;
		next2.ah_attr.grh.sgid_index = 0;
	}

	struct ibv_qp_attr next1 = {
			.qp_state = IBV_QPS_RTR,
			.path_mtu = IBV_MTU_1024,
			.dest_qp_num = qp1->qp_num,
			.rq_psn = psn1,
			.max_dest_rd_atomic = 1,
			.min_rnr_timer = 12,
			.ah_attr = {
				.is_global = 0,
				.dlid = port1.lid,
				.sl = 0,
				.src_path_bits = 0,
				.port_num = port,
			}
	};

	if(gid1.global.interface_id) {
		next1.ah_attr.is_global = 1;
		next1.ah_attr.grh.hop_limit = 1;
		next1.ah_attr.grh.dgid = gid1;
		next1.ah_attr.grh.sgid_index = 0;
	}

	if(ibv_modify_qp(qp2, &next1,
		IBV_QP_STATE |
		IBV_QP_AV |
		IBV_QP_PATH_MTU |
		IBV_QP_DEST_QPN |
		IBV_QP_RQ_PSN |
		IBV_QP_MAX_DEST_RD_ATOMIC |
		IBV_QP_MIN_RNR_TIMER)) {
		printf("Failed to modify verbs2 to ready\n");
		return 1;
	}

	DPRINTF("verbs2 RTR\n");

	if(ibv_modify_qp(qp1, &next2,
		IBV_QP_STATE |
		IBV_QP_AV |
		IBV_QP_PATH_MTU |
		IBV_QP_DEST_QPN |
		IBV_QP_RQ_PSN |
		IBV_QP_MAX_DEST_RD_ATOMIC |
		IBV_QP_MIN_RNR_TIMER)) {
		printf("Failed to modify verbs1 to ready\n");
		return 1;
	}

	DPRINTF("verbs1 RTR\n");

	next2.qp_state = IBV_QPS_RTS;
	next2.timeout = 14;
	next2.retry_cnt = 7;
	next2.rnr_retry = 7;
	next2.sq_psn = psn1;
	next2.max_rd_atomic = 1; 

	if(ibv_modify_qp(qp1, &next2,
		IBV_QP_STATE |
		IBV_QP_TIMEOUT |
		IBV_QP_RETRY_CNT |
		IBV_QP_RNR_RETRY |
		IBV_QP_SQ_PSN |
		IBV_QP_MAX_QP_RD_ATOMIC)) {
		printf("Failed again to modify verbs1 to ready\n");
		return 1;
	}

	DPRINTF("verbs1 RTS\n");

	next1.qp_state = IBV_QPS_RTS;
	next1.timeout = 14;
	next1.retry_cnt = 7;
	next1.rnr_retry = 7;
	next1.sq_psn = psn2;
	next1.max_rd_atomic = 1; 

	if(ibv_modify_qp(qp2, &next1,
		IBV_QP_STATE |
		IBV_QP_TIMEOUT |
		IBV_QP_RETRY_CNT |
		IBV_QP_RNR_RETRY |
		IBV_QP_SQ_PSN |
		IBV_QP_MAX_QP_RD_ATOMIC)) {
		printf("Failed again to modify verbs2 to ready\n");
		return 1;
	}

	DPRINTF("verbs2 RTS\n");

        printf("Performing RDMA first.\n");
	iterations = atoi(argv[3]);

	getrusage(RUSAGE_SELF, &usage);
	ustart = usage.ru_utime;
	sstart = usage.ru_stime;

	gettimeofday(&tstart, NULL);

	while(iterations-- > 0) {
		sge.addr   = (uintptr_t) buf1;
		sge.length = bytes; 
		sge.lkey   = mr1->lkey;

		send_wr.wr_id		    = 1;
		send_wr.opcode		    = IBV_WR_RDMA_WRITE;
		send_wr.sg_list		    = &sge;
		send_wr.num_sge		    = 1;
		send_wr.send_flags          = IBV_SEND_SIGNALED;
		send_wr.wr.rdma.rkey 	    = mr2->rkey;
		send_wr.wr.rdma.remote_addr = (uint64_t) buf2; 

		DPRINTF("Iterations left: %d\n", iterations);
		if (ibv_req_notify_cq(cq1, 0))
			return 1;

		DPRINTF("Submitting local RDMA\n");
		gettimeofday(&start, NULL);
		if (ibv_post_send(qp1, &send_wr, &bad_send_wr))
			return 1;

		DPRINTF("RDMA posted %p %p\n", &send_wr, bad_send_wr);

		DPRINTF("blocking...\n");
		if(ibv_get_cq_event(comp_chan1, &evt_cq, &cq_context)) {
			printf("failed to get CQ event\n");
			return 1;
		}
		gettimeofday(&stop, NULL);
		timersub(&stop, &start, &diff);

		DPRINTF("RDMA took: %lu us\n", diff.tv_usec);

		ibv_ack_cq_events(evt_cq, 1);

		DPRINTF("got event\n");

		n = ibv_poll_cq(cq1, 1, &wc);
		if (n > 0) {
			DPRINTF("return from poll: %lu\n", wc.wr_id);
			if (wc.status != IBV_WC_SUCCESS) {
				printf("poll failed %s\n", ibv_wc_status_str(wc.status));
				return 1;
			}

			if (wc.wr_id == 1) {
				DPRINTF("Finished %d bytes %d %d\n", n, buf1[bytes - 1], buf2[bytes - 1]);
			} else {
				printf("didn't find completion\n");
			}
		}

		if (n < 0) {
			printf("poll returned error\n");
			return 1;
		}

		DPRINTF("Poll returned %d bytes %d %d\n", n, buf1[0], buf2[0]);

    	}

	gettimeofday(&tend, NULL);

	getrusage(RUSAGE_SELF, &usage);
	uend = usage.ru_utime;
	send = usage.ru_stime;

	save_diff = 0;
	timersub(&uend, &ustart, &diff);
	save_diff += diff.tv_usec;
	printf("User CPU time: %lu us\n", diff.tv_usec);
	timersub(&send, &sstart, &diff);
	save_diff += diff.tv_usec;
	printf("System CPU time: %lu us\n", diff.tv_usec);
	timersub(&tend, &tstart, &diff);
	printf("Sleeping time: %lu us\n", diff.tv_usec - save_diff);
	printf("Wall clock CPU time: %lu us\n", diff.tv_usec);

	iterations = atoi(argv[3]);

        printf("Now using the CPU instead....\n");

	getrusage(RUSAGE_SELF, &usage);
	ustart = usage.ru_utime;
	sstart = usage.ru_stime;

	gettimeofday(&tstart, NULL);

	while(iterations-- > 0) {
		DPRINTF("Repeating without RDMA...\n");

		gettimeofday(&start, NULL);

		memcpy(buf2, buf1, bytes);

		gettimeofday(&stop, NULL);
		timersub(&stop, &start, &diff);
		DPRINTF("Regular copy too took: %lu us\n", diff.tv_usec);
	}

	gettimeofday(&tend, NULL);

	getrusage(RUSAGE_SELF, &usage);
	uend = usage.ru_utime;
	send = usage.ru_stime;

	save_diff = 0;
	timersub(&uend, &ustart, &diff);
	save_diff += diff.tv_usec;
	printf("User CPU time: %lu us\n", diff.tv_usec);
	timersub(&send, &sstart, &diff);
	save_diff += diff.tv_usec;
	printf("System CPU time: %lu us\n", diff.tv_usec);
	timersub(&tend, &tstart, &diff);
	printf("Sleeping time: %lu us\n", diff.tv_usec - save_diff);
	printf("Wall clock CPU time: %lu us\n", diff.tv_usec);
	return 0;
}
