#!/bin/bash
scheduler_host="tian01"
scheduler_port="9000"

processLine(){
	line="$@" # get all args
  	arg1=$(echo $line | awk '{ print $1 }')
  	arg2=$(echo $line | awk '{ print $2 }')
  	arg3=$(echo $line | awk '{ print $3 }')

#	echo $arg1
#	echo $arg2
#	echo $arg3
	usleep $arg3
	cmd="/home/tian/testAPI/test_bench $arg1 $arg2 $scheduler_host $scheduler_port"
	echo $cmd; eval $cmd
}

node=$1

FILE="job_$node.txt"
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
while read -r line
do
	processLine $line
done
exec 0<&3
IFS=$BAKIFS
exit 0
