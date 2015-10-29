#include <iostream>
#include <stdio.h>
#include <ctime>
#include <stdlib.h>
#include <omp.h>
#define N 50000000
using namespace std;
double vector1[N], vector2[N];
int main(int argc, char ** argv){
	clock_t t1,t2;
	double wtime1, wtime2;

	t1=clock();

	wtime1 = omp_get_wtime ( );
	
	for(int i=0;i<N;i++){
		vector1[i]=rand()% 100/3;
		vector2[i]=rand()% 200/3;
	}

#pragma omp parallel for
	for(int i=0;i<N;i++){
//		for(int j=0;j<N;j++)
		vector1[i]+=vector2[i]*vector1[i]-vector1[i]/5.0-vector2[i]/3.1;
	}

	wtime2 = omp_get_wtime ( );

	t2=clock();
	cout<<"wtime: "<<(wtime2-wtime1)<< " secs"<<endl; 
	cout<<"time= "<<(t2-t1)/(double)CLOCKS_PER_SEC<<" secs "<<endl;
	return 0;
}
