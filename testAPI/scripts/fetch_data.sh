#!/bin/bash
node=$1
t_dir="${node}-data"
rm -rf $t_dir 
mkdir $t_dir
t_file="${node}-l"
grep "OPEN" ${t_file}.log | awk '{print $4}' > $t_dir/${t_file}-open.log
grep "EXE" ${t_file}.log | awk '{print $6}' > $t_dir/${t_file}-execute.log
grep "CLOSE" ${t_file}.log | awk '{print $8}'> $t_dir/${t_file}-close.log
