#include <xmlrpc.h>
#include <xmlrpc_client.h>
#include <memory.h>

#include "client.h"

#define NAME       "fpga_rdma"
#define VERSION    "0.1"
const char * fpga_scheduler_url = "http://10.0.0.50:9000";
void die_if_fault_occurred (xmlrpc_env *env);
void client_rpc_request(void *rpc_param, char * job_id);
void client_rpc_notify(void *acc_context);

void * fpga_acc_open(struct acc_context_t * acc_context, char * acc_name, unsigned int in_buf_size, unsigned int out_buf_size, unsigned int real_in_size, char * fpga_scheduler_url);
unsigned long fpga_acc_do_job (struct acc_context_t * acc_context, const char * param, unsigned int job_len, void ** result_buf);
void fpga_acc_close(struct acc_context_t * acc_context);

void * fpga_acc_open(struct acc_context_t * acc_context, char * acc_name, unsigned int in_buf_size, unsigned int out_buf_size, unsigned int real_in_size, char * fpga_scheduler_url)
{
    //copy paramters to rpc_param_context
    strcpy (acc_context->acc_name, acc_name);
    strcpy (acc_context->rpc_param.acc_name, acc_name);

    acc_context->in_buf_size = in_buf_size;
    acc_context->rpc_param.in_buf_size = in_buf_size;

    acc_context->out_buf_size = out_buf_size;
    acc_context->rpc_param.out_buf_size = out_buf_size;

    acc_context->real_in_buf_size = real_in_size;
    acc_context->rpc_param.real_in_buf_size = real_in_size;

    strcpy(acc_context->rpc_param.url, fpga_scheduler_url);
    acc_context->execution_time = 0.0;
    struct timeval start, end, dt;
    gettimeofday(&start, NULL);

    //request rpc server to open FPGA device as well as a rdma server listening port
    client_rpc_request (&(acc_context->rpc_param), acc_context->job_id);
    if (strcmp(acc_context->rpc_param.status,"0") == 0) {
        //set up rdma connection with remote server;
        printf("ready to setup rdma connection\n");
        client_rdma_setup_connection(&(acc_context->rpc_param));

	acc_context->in_buf = acc_context->rpc_param.in_buf;
    }
    else if (strcmp(acc_context->rpc_param.status,"1") == 0) {
        printf("Fail to open FPGA device.\n");
        return NULL;
    }
    else if (strcmp(acc_context->rpc_param.status,"2") == 0) {
        printf("Fail to open remote RDMA device.\n");
        return NULL;
    }
    else if (strcmp(acc_context->rpc_param.status,"3") == 0) {
	printf("Open Local FPGA device\n");
	int section_id = atoi(acc_context->rpc_param.section_id);
	acc_context->in_buf = pri_acc_open(&(acc_context->rpc_param.acc_handler), acc_context->acc_name, acc_context->in_buf_size, acc_context-> out_buf_size, section_id);

   }
    gettimeofday(&end, NULL);
    timersub(&end, &start, &dt);
    long open_usec = dt.tv_usec + 1000000 * dt.tv_sec;
    acc_context->open_time = open_usec;
    acc_context->total_time = 0;
    acc_context->total_time += open_usec;
    return acc_context->in_buf;
}

unsigned long fpga_acc_do_job (struct acc_context_t * acc_context, const char * param, unsigned int job_len, void ** result_buf)
{
    unsigned long result_buf_size;
    struct timeval start, end, dt;
    gettimeofday(&start, NULL);
    if(strcmp(acc_context->rpc_param.status, "3") == 0) {
	//printf("Do local FPGA job\n");
	result_buf_size =  (acc_do_job(&(acc_context->rpc_param.acc_handler), param, job_len, result_buf));
    }

    else if(strcmp(acc_context->rpc_param.status, "0") == 0) {
	//printf("Do remote job\n");
    	result_buf_size = (client_rdma_data_transfer(&(acc_context->rpc_param), result_buf));
    }
    gettimeofday(&end, NULL);
    timersub(&end, &start, &dt);
    long exe_usec = dt.tv_usec + 1000000 * dt.tv_sec;
    acc_context->execution_time = exe_usec;
    acc_context->total_time += exe_usec;
    return result_buf_size;
}

void fpga_acc_close(struct acc_context_t * acc_context)
{
    if(strcmp(acc_context->rpc_param.status,"3") == 0) {
	printf("Close local fpga \n");
        acc_close(&(acc_context->rpc_param.acc_handler));
    }
    else if(strcmp(acc_context->rpc_param.status, "0") == 0) {
	printf("Close remote fpga\n");
    	client_rdma_disconnect(&(acc_context->rpc_param));
    }
    client_rpc_notify(acc_context);
    return;
}


void die_if_fault_occurred (xmlrpc_env *env)
{
    /* Check our error-handling environment for an XML-RPC fault. */
    if (env->fault_occurred) {
        fprintf(stderr, "XML-RPC Fault: %s (%d)\n", env->fault_string, env->fault_code);
        exit(1);
    }
}

void client_rpc_notify(void * acc_ctx){
    xmlrpc_env env;
    xmlrpc_value *result;

    const char *status;
    char execution_time[256];
    char open_time[256];
    char total_time[256];
    char job_id[256];

    struct acc_context_t *acc_context = (struct acc_context_t *)acc_ctx;
    long f_execution_time = acc_context->execution_time;
    long f_open_time = acc_context->open_time;
    long f_total_time = acc_context->total_time;
    struct rpc_param_t *my_context =&(acc_context->rpc_param);

    /* start up our xml-rpc client library. */
    xmlrpc_client_init(XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION);
    xmlrpc_env_init(&env);

    /* call our xml-rpc server. */
    sprintf(open_time, "%ld", f_open_time);
    sprintf(execution_time, "%ld", f_execution_time);
    sprintf(total_time, "%ld", f_total_time);
    printf("time are %ld, %ld, %ld\n", f_open_time, f_execution_time, f_total_time);
    sprintf(job_id, "%s", acc_context->job_id);
    printf("job_id = %s, ip = %s, section_id = %s\n", job_id, my_context->ipaddr, my_context->section_id);
    result=xmlrpc_client_call(&env, my_context->url, "on_receive_complete", "(ssss)", job_id, open_time, execution_time, total_time);
    die_if_fault_occurred(&env);

    /* parse our result value. */
    xmlrpc_parse_value(&env,result,"{s:s,*}", "status", &status);

    if(strcmp(status, "1"))
    	printf("notification completed successfully.\n");

    xmlrpc_DECREF(result);
    xmlrpc_env_clean(&env);
    xmlrpc_client_cleanup();

}

void client_rpc_request(void *rpc_param, char *job_id){

    xmlrpc_env env;
    xmlrpc_value *result;
    //xmlrpc_int32 status;
    //xmlrpc_int32 section_id;

    const char *port;
    const char *ip;
    const char *section_id;
    const char *status;
    const char *current_job_id;
    char in_buf_size[16];
    char real_in_buf_size[16];
    char out_buf_size[16];
    char client_ip_addr[16];

    struct rpc_param_t *my_context =(struct rpc_param_t *) rpc_param;

    /* start up our xml-rpc client library. */
    xmlrpc_client_init(XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION);
    xmlrpc_env_init(&env);

    /* call our xml-rpc server. */
    sprintf(in_buf_size, "%d", my_context->in_buf_size);
    sprintf(real_in_buf_size, "%d", my_context->real_in_buf_size);
    sprintf(out_buf_size, "%d", my_context->out_buf_size);
    sprintf(client_ip_addr, "%s", get_local_ip_addr());
    result=xmlrpc_client_call(&env, my_context->url, "on_receive_request", "(sssss)", client_ip_addr, real_in_buf_size, in_buf_size, out_buf_size, my_context->acc_name);
    die_if_fault_occurred(&env);

    /* parse our result value. */
    xmlrpc_parse_value(&env,result,"{s:s,s:s,s:s,s:s,s:s,*}", "ip", &ip, "section_id", &section_id, "port", &port, "ifuse", &status, "job_id", &current_job_id);

	strcpy(my_context->port, port);
	strcpy(my_context->ipaddr, ip);
        strcpy(my_context->status, status);
        strcpy(my_context->section_id, section_id);
	strcpy(job_id, current_job_id);
	//printf("section_id = %s, port = %s, ip = %s, status = %s\n",my_context->section_id, my_context->port, my_context->ipaddr, my_context->status);


    xmlrpc_DECREF(result);
    xmlrpc_env_clean(&env);
    xmlrpc_client_cleanup();

};


