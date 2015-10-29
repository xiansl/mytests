#!/bin/bash
#!/usr/bin/php

rm -r log_file2
mkdir log_file2

./set_job.py global exponential 1000
for i in {0..9};
do
	./simulation_process.py SJF >> log_file2/global_wk.log 2>&1
done

#for i in {0..9};
#do
#	./simulationprocess.py random >> log_file2/local_wk.log 2>&1
#done
#
#for i in {0..9};
#do
#	./simulationprocess.py test2 >> log_file/local_wk.log 2>&1
#done

#grep "FAIR_SHARING" log_file/local_wk.log | grep "SNP"> log_file/fair_snp.log
#grep "FAIR_SHARING" log_file/local_wk.log | grep "Network"> log_file/fair_network.log
#grep "RANDOM" log_file/local_wk.log | grep "SNP"> log_file/random_snp.log
#grep "RANDOM" log_file/local_wk.log | grep "Network"> log_file/random_network.log
#grep "TEST_2" log_file/local_wk.log | grep "SNP"> log_file/test2_snp.log
#grep "TEST_2" log_file/local_wk.log | grep "Network"> log_file/test2_network.log
grep "SJF" log_file2/global_wk.log | grep "SNP" > log_file2/sjf_snp.log
grep "SJF" log_file2/global_wk.log | grep "Makespan" > log_file2/sjf_makespan.log
grep "SJF" log_file2/global_wk.log | grep "SSP" > log_file2/sjf_ssp.log
