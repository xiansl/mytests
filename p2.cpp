#include<iostream>
#include <fstream>
#define N 1000
using namespace std;
int maxnum(int n, int a[N]){
	cout<<"n="<<n<<endl;
	if(n==1) return a[n-1];
	int aaa = maxnum(n-1,a);
	if(aaa<0) return a[n-1];
	else return (a[n-1]+aaa);
}
int main(int argc, char **args){
int a,b,flag;
int arr[N];
ifstream myfile("Input.txt");
ofstream myfile2("Output.txt");
myfile>>a;
for(int i=0;i<a;i++){
myfile>>arr[i];
}
flag=maxnum(a,arr);
cout<<flag;
return 0;
}
