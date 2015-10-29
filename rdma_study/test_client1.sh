#!/bin/bash


port=$1

echo "port is ${port}"

rm -f client1.log

for((b=1; b<=1024; b=b*2))
do
for i in {1..5};
do
	cmd="./client1 r2 ${port} $b"
    echo $cmd 2>&1 | tee -a client1.log
    eval $cmd 2>&1 | tee -a client1.log
done
done

#grep "job" client.log | awk '{print $7}'| awk -F'=' '{print $2}'> client.log.txt
#grep "completion" client.log > client.log_error.txt
