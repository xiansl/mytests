//#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>
#include <inttypes.h>
#include <sched.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>

#include "workThd.h"


int main(int argc, char **argv) {
    int t, opt;
    int acc_id, iters, cpu_set, input_size, random_iters;
    cpu_set_t cpuset;
    pthread_t threads;
    WorkThdData thdData;
    pthread_attr_t attr;
    void * status;

	struct timeval wait_tv;
	fd_set rdfds;

	double bandwidth, time_average;
#ifdef LOCAL_TSC
	double cycle_dt;
	uint64_t cycle_average, temp_dt;
#endif

	char stop = 0;
    char acc_name[64] = "";

    /*Parameter init*/
    acc_id = 1;
    iters = -1;					// loop endless
    cpu_set= 0;
    input_size = 1;
	random_iters = 0;
    while ((opt = getopt(argc, argv, "M:I:i:a:c:r:")) != -1)
    {
        switch(opt)
        {
          case 'i':
            iters = strtol(optarg, NULL, 0);
            break;

          case 'a':
            acc_id = strtol(optarg, NULL, 0);
            break;

		  case 'M':
			strcpy(acc_name, optarg);
			break;

		  case 'I':
            input_size = strtol(optarg, NULL, 0);
            if (input_size <= 0) {
                printf("ERROR: Size of input should be greater than 0.\n");
                exit(0);
            }
            break;
          case 'c':
            cpu_set= strtol(optarg, NULL, 0);
            break;
		
		  case 'r':
			random_iters = strtol(optarg, NULL, 0);
			break;
            
          default:
            break;
        }
    }

    /*Create threads and do tests*/
    pthread_t myself = pthread_self();
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	CPU_ZERO(&cpuset);
	thdData.acc_id = acc_id;
	strcpy(thdData.acc_name, acc_name);
	thdData.iters = iters;
	thdData.input_size = input_size;
	thdData.cpu_set= cpu_set;
	thdData.random_iters = random_iters;

	pthread_create(&threads, NULL, worker_thread, (void *)(&thdData));


	printf("\nEnter \"z\" key to stop normally, \"f\" key to force stop\n");

	while (1) {
		if (ack == 1)
			break;

		FD_ZERO(&rdfds);
		FD_SET(0, &rdfds);
		wait_tv.tv_sec = 0;
		wait_tv.tv_usec = 300000;
		t = select (1, &rdfds, NULL, NULL, &wait_tv);

		if (t < 0) {
			printf ("wait for input error.\n");
			break;
		}

		if (t > 0) {
			stop = getchar ();
			if (stop == 'z')
				run = 1;
			else if (stop == 'f')
				run = 0;
		}

		if (job_send) {
			job_send = 0;
			bandwidth = ((double)pay_load)/(1024*1024)/((1.0 * job_usec)/1000000.0);
#ifdef LOCAL_TSC
			if (job_num <= 102400) {
				cycle_average = 0;
				for (t = 0; t < job_num; t ++)
					cycle_average += job_cycles [t];
				cycle_average /= job_num;

				cycle_dt = 0.0;
				for (t = 0; t < job_num; t ++) {
					if (job_cycles [t] > cycle_average)
						temp_dt = job_cycles [t] - cycle_average;
					else
						temp_dt = cycle_average - job_cycles [t];

					cycle_dt += ((double)temp_dt) * ((double)temp_dt);
				}
			}

			time_average = ((double)cycle_average) / (CPU_FREQENCY*1000000.0);
			cycle_dt /= ((double)job_num);
			cycle_dt = sqrt (cycle_dt);
			cycle_dt /= (CPU_FREQENCY*10000.0);
			printf("BW: %7.2fMB/s --- A: %.3fms --- V: %.2f%\n", bandwidth, time_average, cycle_dt/time_average);
#else
			time_average = (1.0 * job_usec) / (1000.0 * job_num);
			printf("BW: %7.2fMB/s --- A: %.3fms\n", bandwidth, time_average);
#endif
		}
	}

    /* Join pthreads */
    pthread_attr_destroy(&attr);
    pthread_join(threads, &status);

    return 0;
}
