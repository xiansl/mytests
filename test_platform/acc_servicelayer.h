#include "fpga-sim/driver/fpga-libacc.h"
#define MAX_BUFFER_SIZE 4096

struct acc_context_t {
    char acc_name [16];
    char job_id[256];
    unsigned int in_buf_size;
    unsigned int out_buf_size;
    unsigned int real_in_buf_size;
    void * in_buf;
    void ** result_buf;
    long execution_time;
    long total_time;
    long open_time;
    struct rpc_param_t rpc_param;  
};


void * fpga_acc_open(struct acc_context_t * acc_context, char * acc_name, unsigned int in_buf_size, unsigned int out_buf_size, unsigned int real_in_size, char * fpga_scheduler_url);

unsigned long fpga_acc_do_job (struct acc_context_t * acc_context, const char * param, unsigned int job_len, void ** result_buf);

void fpga_acc_close(struct acc_context_t * acc_context);

