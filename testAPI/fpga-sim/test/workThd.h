#ifndef WORK_THD_H
#define WORK_THD_H

#define CPU_FREQENCY 3.47
#define LOCAL_TSC

typedef struct WorkThdData {
    char acc_name[64];
    int acc_id;
    int iters;
    int input_size;
    int cpu_set;
	int random_iters;
} WorkThdData;

extern int run;
extern int ack;
extern int job_send;
extern uint64_t pay_load;
extern uint64_t job_usec;
extern int job_num;
extern uint64_t * job_cycles;

extern void *worker_thread(void *data);

#endif  /* #ifndef WORK_THD_H */
