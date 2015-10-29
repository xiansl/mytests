#!/bin/bash


port=$1

echo "port is ${port}"

> client.log

for i in {1..20};
do
	./client 192.168.3.2 ${port} >> client.log 2>&1
	sleep 1
done

grep "job" client.log | awk '{print $7}'| awk -F'=' '{print $2}'> client.log.txt
grep "completion" client.log > client.log_error.txt
