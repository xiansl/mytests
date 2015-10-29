#!/bin/bash


algorithm=$1
mean=$2

echo "algorithm is ${algorithm}"
echo "mean is ${mean}"

grep "OPEN" logfile$mean/${algorithm}/${algorithm}.log > logfile$mean/${algorithm}/total_time.log
grep "COMPLETES" logfile$mean/${algorithm}/${algorithm}.log> logfile$mean/${algorithm}/make_span.log
grep "ARRIVES" logfile$mean/${algorithm}/${algorithm}.log> logfile$mean/${algorithm}/arrive.log
grep "IDEAL" logfile$mean/${algorithm}/${algorithm}.log> logfile$mean/${algorithm}/ideal.log
