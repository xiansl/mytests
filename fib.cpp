#include<iostream>
using namespace std;
int main(int argc, char **args){
int a,b,c;
a=1;
b=1;
cout<<a<<" "<<b<<" ";
for(int i=1;i<19;i++){
c=a+b;
cout<<c<<" ";
a=b;
b=c;
}
cout<<endl;
return 0;
}
