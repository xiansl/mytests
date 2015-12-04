#!/bin/bash

# clock_id :
# 0 : 500 MHz
# 1 : 333 MHz
# 2 : 250 MHz
# 3 : 200 MHz
# 4 : 167 MHz
# 5 : 100 MHz
# 6 : 83.3 MHz
# 7 : 50 MHz


# syntax:
# ./clock -p port_id -f clock_id

# example :
# "./clock -p 0 -f 5" will change the clock freqency in port0 to 100MHz

port=255
clock=7

while getopts "p:f:" arg
do
	case $arg in
		p)
			port=$OPTARG
			;;
		f)
			clock=$OPTARG
			;;
		?)
			echo "unknow argument"
			exit 1
			;;
	esac
done

cd `dirname $0`
./pcie_io 2 48 0x0${clock}0${port}
