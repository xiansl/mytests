#!/bin/bash


port=$1
recbuf=$2

recbuf=4194304

echo "port is ${port}"
echo "recbuf is ${recbuf}"


buflog=clientbuf.log
buftxt=${buflog}.txt
buferr=${buflog}.err

#clear file
> ${buflog}
> ${buftxt}
> ${buferr}

for ((i=64; i<=128*1024*1024; i=i*2))
do
	send_buf=${i}
	echo "current send_buf is ${send_buf}"
	./client 192.168.3.2 ${port} ${send_buf} ${recbuf} >> ${buflog}  2>&1

	echo "sleep 1s, run next"
	sleep 1
done

grep "job" ${buflog} | awk '{print $7}'| awk -F'=' '{print $2}'> ${buftxt} 
grep "completion" ${buflog} > ${buferr}

echo "Done"
