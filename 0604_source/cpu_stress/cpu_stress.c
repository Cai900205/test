#include "help.h"
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
#define CPU_TOTAL_NUM 100


struct test_stress{
      int bind; 
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
   // CPU_SET(test->rank,&cpuset);
    if(test->bind)
    {
        if(sched_getaffinity(0,sizeof(cpuset),&cpuset) == -1)
        {
           printf("warning: cound not get cpu affinity!\n");
           return (void*)-1;
        }
	    result = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
        if (result)
	    {
		   get_time(time_str);
		   printf("[CPU-STRESS-%s]: Bind cpu  failed\n",
		   		time_str);
           fflush(stdout);
		   return (void*)-1;
	    }

    }
    struct timeval tm_start,tm_end;
	float base = 3.1415926;
    uint64_t i=0;
    uint64_t testtimes=0;
    gettimeofday(&tm_start,NULL);
    double diff=0;
    for(i=0;((i<test->times)||(!test->times));i++)
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
        diff = ((tm_end.tv_sec -tm_start.tv_sec)*1000000+(tm_end.tv_usec-tm_start.tv_usec))/1000000.0;
        if((diff > test->time)&&(test->times))
        {
            printf("cpu stress time sucessful!\n");
            fflush(stdout);
            break;
        }
        if(!((i+1)%10000000))
        {
            printf("cpu stress passes sucessful!\n");
        }
    }
    if(i==test->times)
    {
        printf("cpu stress passes sucessful!\n");
        fflush(stdout);
    }
    return (void*)0;
}


int main(int argc, char** argv)
{
	int result = -1;
    int workers = 8;
    uint64_t passes = 300,time=10;
	char time_str[30];
	int back[workers];
    int i;
    int interval = 1,bind=0;
    int maxworkers=1;
    maxworkers=sysconf(_SC_NPROCESSORS_ONLN);

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
		else if (!strcmp(arg, "--interval") && i + 1 < argc)
		{
			interval = atoi(argv[++i]);
		}
		else if (!strcmp(arg, "--bind"))
		{
            bind=1;
		}
		else if (!strcmp(arg, "--time") && i + 1 < argc) 
        {
            time = strtoull(argv[++i],NULL,10);
        }
		else if (!strcmp(arg, "--help")) 
        { 
            usage();
            fflush(stdout);
            return 0;
        }
		else if ( !strcmp(arg,"--version")) 
        { 
            printf("CPU STRESS VERSION:POWERPC-CPU_STRESS-V1.00\n");
            fflush(stdout);
            return 0;
        }
        else
        {
			printf("Unknown option: %s\n", arg);
            usage();
            fflush(stdout);
			return -1;
		}
	}

	pthread_t threads[24];
    struct test_stress work[24];
    if(workers > maxworkers|| workers<1)
    {
        printf("workers too much or too small!\n");
        fflush(stdout);
        return -1;
    }
	for ( i = 0; i < workers; i++)
	{
        work[i].times=passes;
        work[i].time=time;
        work[i].bind=bind;
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
	printf("[CPU-STRESS-%s]: Stress test by %d threads sucessful\n",
			time_str, workers);
    fflush(stdout);
	return 0;
}
