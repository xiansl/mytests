#!/bin/bash

port=$1
recbuf=$2

recbuf=4194304

echo "port is ${port}"
echo "recbuf is ${recbuf}"

server_ip="192.168.3.2"

curdir=`pwd`

DATETIME=$(date +%Y%m%d_%H%M%S)
logdir=client_logdir_${DATETIME}
if [ ! -d $logdir ];then
	mkdir -p $logdir
fi

pushd ${logdir}

function testone()
{
	cur_port=$1
	cur_recbuf=$2
	cur_sendbuf=$3

	echo "${cur_port} ${cur_recbuf} ${cur_sendbuf}"


	prefix=clientbuf_port${cur_port}_rb${cur_recbuf}_sb${cur_sendbuf}

	buflog=${prefix}.log

	buftxt=${buflog}.txt
	buferr=${buflog}.err
	
	echo ${buflog}.log

	#clear file
	> ${buflog}
	> ${buftxt}
	> ${buferr}

	for ((cnt=0;cnt<10;cnt++))
	do
		echo "run count ${cnt}"
		set -x
		${curdir}/client ${server_ip} ${cur_port} ${cur_sendbuf} ${cur_recbuf}  >> ${buflog}  2>&1
		set +x

		echo "sleep 1s"
		sleep 1
	done

	grep "job" ${buflog} | awk '{print $7}'| awk -F'=' '{print $2}'> ${buftxt}
	grep "completion" ${buflog} > ${buferr}

}



function testall()
{
	mport=$1
	mrecbuf=$2

	for ((msendbuf=64; msendbuf<=1024*1024*1024; msendbuf=msendbuf*2))
	do
		echo "current send_buf is ----->>> ${msendbuf}"

		testone  ${mport} ${mrecbuf} ${msendbuf}

		echo "sleep 1s, run next buf test"
		sleep 1
	done
}



#start here
testall ${port}  ${recbuf}


popd
echo "Done"
echo "new log dir ${logdir}"
