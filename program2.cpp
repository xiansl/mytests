#include<iostream>
#include <fstream>
#define N 1000
using namespace std;
int maxnum(int n, int a[N]){
int aa;
if(n==1)
return a[0];
else aa=maxnum(n-1,a);
if(aa<0)return a[n-1];
else return(aa+a[n-1]);
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
for(int i=1;i<=a;i++){
b=maxnum(i,arr);
if(flag<b)
flag=b;}
myfile2<<flag<<endl;
return 0;
}
