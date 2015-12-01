#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

int hypervisor_decision(char *acc_name, unsigned int in_buf_size){
    double acc_bw = 1200; // M bytes
    if (strcmp(acc_name, "FFT") == 0){
        acc_bw = 150;
    }
    double current_time = in_buf_size/acc_bw;
    double tag_time = 4096*1024/1200;

    if(current_time < tag_time)
        return 1;  //return 0 if want to launch this job on FPGA platform
    else
        return 0;//return 1 if want to run it via software implementation
}

void software_do_job(char *acc_name, unsigned int in_buf_size){
    return;
}

