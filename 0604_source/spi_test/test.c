/*************************************************************************
	> File Name: test.c
	> Author: 
	> Mail: 
	> Created Time: Tue 19 May 2015 10:28:41 AM CST
 ************************************************************************/

#include<stdio.h>
#include<string.h>

int main(int argc,char **agrv)
{
   int arr[10];
   char str[10];

   int p=1234;
   memset(arr,p ,40);
   memset(str,p,10);
   int i=0;
   for(i=0; i<10 ;i++)
    {
        printf("%08x,%02x\n",arr[i],str[i]);
    }
    return 0;
}
