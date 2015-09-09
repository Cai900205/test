#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <error.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>

#define __USE_GNU
#include <limits.h>
#include <signal.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/time.h>

#include "util.h"
#define CPU_TOTAL_NUM 24


struct test_stress{
      int rank; 
      uint64_t times;
      uint64_t time;
};



static void* loop_aux(void* arg)
{
	int result = -1;
	struct test_stress *test=arg;
	char time_str[30];
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(test->rank,&cpuset);
	result = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
    if (result)
	{
		get_time(time_str);
		printf("[CPU-STRESS-%s]: Bind cpu for thread-%d failed\n",
				time_str, test->rank);
        fflush(stdout);
		return (void*)-1;
	}
    struct timeval tm_start,tm_end;
	float base = 3.1415926;
    uint64_t i=0;
    uint64_t testtimes=0;
    for(i=0;i<test->times;i++)
    {
        gettimeofday(&tm_start,NULL);
        uint64_t diff=0;
        while(diff < test->time)
        {
		    if (base > 20130605)
		    {
			    base = base * 3.1415926;
		    }
		    else
		    {
			    base = base / 3.1415926;
		    }
            gettimeofday(&tm_end,NULL);
            diff = (tm_end.tv_sec -tm_start.tv_sec);
        }
        printf("cpu:%d cpu stress sucessful!\n",test->rank);
        fflush(stdout);
	}
	
    return (void*)0;
}

int main(int argc, char** argv)
{
	int result = -1;
    int workers = 8;
    uint64_t passes = 300,time=100;
	char time_str[30];
	int back[workers];
    int i;
    int start = 1;
	for (i = 1; i < argc; i++)
	{
		char* arg = argv[i];
		if (!strcmp(arg, "--passes") && i + 1 < argc)
		{
			passes = strtoull(argv[++i],NULL,10);
		}
		else if (!strcmp(arg, "--workers") && i + 1 < argc)
		{
			workers = atoi(argv[++i]);
		}
		else if (!strcmp(arg, "--startcpu") && i + 1 < argc)
		{
			start = atoi(argv[++i]);
		}
		else if (!strcmp(arg, "--time") && i + 1 < argc) 
        {
            time = strtoull(argv[++i],NULL,10);
        }
        else
        {
			printf("Unknown option: %s\n", arg);
            fflush(stdout);
			return -1;
		}
	}

	pthread_t threads[24];
    struct test_stress work[24];
    if((workers+start) > CPU_TOTAL_NUM)
    {
        printf("workers > CPU_TOTALNUMBERS!\n");
        fflush(stdout);
        return -1;
    }
	for ( i = 0; i < workers; i++)
	{
        work[i].rank=i+start;
        work[i].times=passes;
        work[i].time=time;
		if (pthread_create(&threads[i], NULL, loop_aux,&work[i]))
		{
			get_time(time_str);
			printf("[CPU-STRESS-%s]: Start the cpu stress thread-%d failed\n", 
					time_str, i);
            fflush(stdout);
			return -1;
		}
	}

	for ( i = 0; i < workers; i++)
	{
		pthread_join(threads[i],  (void**)&(back[i]));
		{
			if (back[i] != 0)
			{
				get_time(time_str);
				printf("[CPU-STRESS-%s]: The cpu stress thread-%d return error\n",
						time_str, i);
                fflush(stdout);
				return -1;
			}
		}
	}
	
	get_time(time_str);
	printf("[CPU-STRESS-%s]: Stress test by %d threads for %lld times\n",
			time_str, workers,passes);
    fflush(stdout);
	return 0;
}
