#!/bin/bash

node=$1
scheduler_host=$2
rm -rf ${node}-l.log
touch ${node}-l.log
scheduler_port="9000"

processLine(){
	line="$@" # get all args
  	arg1=$(echo $line | awk '{ print $1 }')
  	arg2=$(echo $line | awk '{ print $2 }')
    arg3=$(echo $line | awk '{ print $3 }')

	sleep $arg3
	/home/tian/testAPI/test_bench $arg1 $arg2 $scheduler_host $scheduler_port & 
}


FILE="job-$node.txt"
if [ ! -f $FILE ]; then
	echo "$FILE: does not exists"
	exit 1
elif [ ! -r $FILE ]; then
	echo "$FILE: can not be read"
	exit 2
fi

BAKIFS=$IFS
IFS=$(echo -en "\n\b")
exec 3<&0
exec 0<"$FILE"
SHELLPID=$$
while read -r line
do
	processLine $line >>${node}-l.log
	#processLine $line
done
exec 0<&3
IFS=$BAKIFS
kill $SHELLPID
exit 0
