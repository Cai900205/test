#include "util.h"
#include "help.h"

int main(int argc, char** argv)
{
    int result = -1;
    int interval = 1;
    int passes = 10;
    int time=10;
    int workers=1;
    int maxworkers=1;
    int bind=0;
    char time_str[30];
    int i;
    for ( i = 1; i < argc; i++)
    {
        char* arg = argv[i];
        if (!strcmp(arg, "--interval") && i + 1 < argc)
        {
            interval = atoi(argv[++i]);
        }
        else if (!strcmp(arg, "--passes") && i + 1 < argc)
        {
            passes = atoi(argv[++i]);
        }
        else if (!strcmp(arg, "--time") && i + 1 < argc)
        {
            time = atoi(argv[++i]);
        }
        else if (!strcmp(arg, "--workers") && i + 1 < argc)
        {
            workers = atoi(argv[++i]);
            if(workers!=maxworkers)
            {
                 printf("Workers error!\n");
                 return -1;
            }
        }
        else if (!strcmp(arg, "--bind"))
        {
            bind=1;
        }
        else if (!strcmp(arg, "--version"))
        {
            printf("RTC VERSION:POWERPC-RTC-V1.00\n");
            return 0;
        }
        else if (!strcmp(arg, "--help"))
        {
            usage();
            return 0;
        }
        else
        {
            printf("Unkown Option:%s\n",arg);
            usage();
            return -1;
        }
    }
    double diff=0;
    struct timeval tm_start,tm_end;
    gettimeofday(&tm_start,NULL);
    while (passes)
    {
        result = system("hwclock");
        if (result)
        {
            get_time(time_str);
            printf("[RTC-%s]: Test RTC for %d-nth time failed\n", time_str, passes);
            fflush(stdout);
            return -1;
        }
        else
        {
            if (passes == 1)
            {
                get_time(time_str);
                printf("[RTC-%s]: Test RTC for %d-nth time success\n", time_str, passes);
                fflush(stdout);
            }
        }
        gettimeofday(&tm_end,NULL);
        diff = ((tm_end.tv_sec -tm_start.tv_sec)*1000000+(tm_end.tv_usec-tm_start.tv_usec))/1000000.0;
        if(diff>time)
        {
            break;
        }
        sleep(interval);
        passes--;
    }

    return 0;
}
