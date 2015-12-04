#!/bin/bash

acc1="AES"
acc2="DTW"
acc3="EC"
acc4="FFT"
acc5="SHA"

rm -r log_file
mkdir log_file

for ((i=1; i<=1024*256; i=i*2))
do	
	./micro_test ${acc1} ${i} >> log_file/${acc1}.log 2>&1
	./micro_test ${acc2} ${i} >> log_file/${acc2}.log 2>&1
	./micro_test ${acc3} ${i} >> log_file/${acc3}.log 2>&1
	./micro_test ${acc4} ${i} >> log_file/${acc4}.log 2>&1
	./micro_test ${acc5} ${i} >> log_file/${acc5}.log 2>&1
done

grep "OPEN" log_file/${acc1}.log | awk '{print $4}' > log_file/${acc1}.open.log
grep "EXECUTION" log_file/${acc1}.log | awk '{print $4}' > log_file/${acc1}.exe.log

grep "OPEN" log_file/${acc2}.log | awk '{print $4}' > log_file/${acc2}.open.log
grep "EXECUTION" log_file/${acc2}.log | awk '{print $4}' > log_file/${acc2}.exe.log

grep "OPEN" log_file/${acc3}.log | awk '{print $4}' > log_file/${acc3}.open.log
grep "EXECUTION" log_file/${acc3}.log | awk '{print $4}' > log_file/${acc3}.exe.log

grep "OPEN" log_file/${acc4}.log | awk '{print $4}' > log_file/${acc4}.open.log
grep "EXECUTION" log_file/${acc4}.log | awk '{print $4}' > log_file/${acc4}.exe.log

grep "OPEN" log_file/${acc5}.log | awk '{print $4}' > log_file/${acc5}.open.log
grep "EXECUTION" log_file/${acc5}.log | awk '{print $4}' > log_file/${acc5}.exe.log
