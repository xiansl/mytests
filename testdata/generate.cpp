#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#define N 20000
using namespace std;
int main(int argc, char **argv){
	fstream f("Input.txt");
	f<<N<<endl;
	for (int i=0; i<N; i++) {
		f<< rand() %200*-100<<" ";
	}
	f<<endl;
	f.close();
	
	fstream ff("Input.txt");
	int n;
	long long a[900];
	ff>>n;
	for(int i=0; i<n; i++)
		ff>>a[i];
	
	long long c=a[0]; 
	for(int i=0; i<n; i++) {
		long long sum=0;
		for(int j=i; j<n; j++) {
			sum+=a[j];
			if(sum>c) c=sum;
		}
	}
	cout<<endl;
	cout<<c<<endl; 	
	return 1;	
}
