#! /bin/bash

declare -i a
for ((i=1; i<=8; i++)) 
do {
	a=${i}*100
	usleep $a
	./test_scheduler AES $i 
}&
done
