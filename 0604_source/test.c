/*************************************************************************
	> File Name: test.c
	> Author: 
	> Mail: 
	> Created Time: Thu 04 Jun 2015 11:13:32 AM CST
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
int main(int argc ,char **argv)
{
    int cpu_num;
    cpu_num = sysconf(_SC_NPROCESSORS_CONF);
    printf("pp:%d\n",cpu_num);
    cpu_num = sysconf(_SC_NPROCESSORS_ONLN);
    printf("avalible:%d\n",cpu_num);
    return 0;
}
