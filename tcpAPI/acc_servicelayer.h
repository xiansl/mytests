#include "fpga-sim/driver/fpga-libacc.h"

struct debug_context_t{
    long open_time;
    long execution_time;
    long close_time;
    long total_time;

};

struct acc_context_t {
    char acc_name [16];
    unsigned int in_buf_size;
    unsigned int out_buf_size;
    unsigned int real_in_buf_size;
    unsigned int real_out_buf_size;
    void * in_buf;
    void ** result_buf;
    struct acc_handler_t acc_handler;
    void * socket_context;
    void * debug_context;
    //struct socket_context_t socket_context;  
    //struct debug_context_t debug_context;
};


void * fpga_acc_open(struct acc_context_t * acc_context, char * acc_name, unsigned int in_buf_size, unsigned int out_buf_size, char * scheduler_host, int scheduler_port);

unsigned long fpga_acc_do_job (struct acc_context_t * acc_context, const char * param, unsigned int job_len, void ** result_buf);

void fpga_acc_close(struct acc_context_t * acc_context);

