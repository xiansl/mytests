#!/bin/bash

# ./reset 0 : will reset the whole FPGA except PCIe

# ./reset 4 : will reset acc0
# ./reset 5 : will reset acc1
# ./reset 6 : will reset acc2
# ./reset 7 : will reset acc3

cd `dirname $0`
./pcie_io 2 32 $1
