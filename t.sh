#!/bin/bash
module load openmpi/1.8.3
cd ~/zzd

host="r31,r32"

#cmd1="mpirun -np 2 -report-bindings -host $host --mca btl self,tcp --mca mpi_leave_pinned 1 ./transfer"
#cmd2="mpirun -np 2 -report-bindings -host $host --mca btl self,openib --mca mpi_leave_pinned 1 ./transfer"
cmd3="mpirun -np 8 -report-bindings -host $host --map-by node -bind-to socket --mca btl self,tcp --mca mpi_leave_pinned 1 ./transfer"
cmd4="mpirun -np 8 -report-bindings -host $host --map-by node -bind-to socket --mca btl self,openib --mca mpi_leave_pinned 1 ./transfer"

for i in {1..10}
do
	#for cmd in "$cmd1" "$cmd2" "$cmd3" "$cmd4"
	for cmd in "$cmd3" "$cmd4"
	do
		echo $cmd
		eval $cmd
	done
done

