#include <stdio.h> 
#include <mpi.h> 
#include <stdlib.h> 
#include <unistd.h> 

int main(int argc, char** argv) 
{ 
 int localID; 
 int numOfPros;
 int loop = 20;
 int skip = 5;
 int i; 
 MPI_Status reqstat; 
 double t_start, t_end, t;
 size_t Gsize = (size_t) 400 * 1024 * 1024; 
 
 char* s_buf = (char*)malloc(Gsize); 
 char* r_buf = (char*)malloc(Gsize); 
 
 MPI_Init(&argc, &argv); 
 MPI_Comm_size(MPI_COMM_WORLD, &numOfPros); 
 MPI_Comm_rank(MPI_COMM_WORLD, &localID); 
 
 if (localID == 0) 
 {
	for (i==0; i<loop+skip; i++) {
 		if(i==skip) {
			t_start = MPI_Wtime();
		}
                MPI_Send(s_buf, Gsize, MPI_CHAR, 1, 1, MPI_COMM_WORLD);
                MPI_Recv(r_buf, Gsize, MPI_CHAR, 1, 1, MPI_COMM_WORLD, &reqstat);
	} 
	t_end = MPI_Wtime();
        t = t_end - t_start;
 } else if (localID == 1) 
 { 
 	for(i=0; i<loop+skip; i++) {
                MPI_Recv(r_buf, Gsize, MPI_CHAR, 0, 1, MPI_COMM_WORLD, &reqstat);
                MPI_Send(s_buf, Gsize, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
 	}
 } 

 if(localID == 0) {
 	printf("time per 400MB: %f secs\n", 1.0 * t / loop);
 }

 if(s_buf!=NULL)
 	free(s_buf);
 if(r_buf!=NULL)
 	free(r_buf);
 MPI_Finalize(); 
 
 return 0; 
 }
