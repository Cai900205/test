#include "help.h"
#include "util.h"
#include "psoc_uart.h"


#define CHIP 							"chip_temp\0"
#define BOARD 							"board_temp\0"

#define TP_WARNING						90

static int board_old = 0;
static int cpu_old = 0;

int main(int argc, char** argv)
{
	int second = 1;
	int interval = 1;
    int time = 10;
	int cpu_tp = 0;
	int board_tp = 0;
	int warning = TP_WARNING;
	char time_str[30];
    int bind=0;
    int workers=1;
	FILE *fp = NULL;
    int i;
    int maxworkers=1;
	for ( i = 1; i < argc; i++)
	{
		char* arg = argv[i];

		if (!strcmp(arg, "--passes") && i + 1 < argc)
		{
			second = atoi(argv[++i]);
		}
		else if (!strcmp(arg, "--time") && i + 1 < argc)
		{
			time = atoi(argv[++i]);
		}
		else if (!strcmp(arg, "--workers") && i + 1 < argc)
		{
			workers=atoi(argv[++i]);
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
		else if (!strcmp(arg, "--interval") && i + 1 < argc)
		{
			interval = atoi(argv[++i]);
		}
		else if (!strcmp(arg, "--version"))
		{
            printf("psoc_tmp VERSION : POWERPC-psoc_tmp-V1.00\n");
            return 0;
		}
		else if (!strcmp(arg, "--help"))
		{
            usage();
            return 0;
		}
		else
		{
			usage();
            printf("Unknown options:%s\n",arg);
			return -1;
		}
	}

    int fd = init_uart_device();
	
	int times = 1;
    int result;
    double diff=0;
    struct timeval tm_start,tm_end;
    gettimeofday(&tm_start,NULL);
	while ((times <= second)||(!second))
	{

        result = get_temperature(fd,times,&cpu_tp,&board_tp);
		if (board_tp > warning || cpu_tp > warning)
		{
			get_time(time_str);
			printf("[TEMPERATURE-%s]: ", time_str);
			printf("The temperature is greater than the threshold: ");
			printf("CPU-%d, Board-%d\n", cpu_tp, board_tp);
			return -1;
		}
		else if (board_old != 0 && ((board_old - board_tp) >= 5 || (board_tp - board_old) >= 5))	
		{
			get_time(time_str);
			printf("[TEMPERATURE-%s]: ", time_str);
			printf("The board temperature change too fast: ");
			printf("Board[%d-%d]\n", board_tp, board_old);
			return -1;			
		}
		else if (cpu_old != 0 && ((cpu_old - cpu_tp) >= 5 || (cpu_tp - cpu_old) >= 5))	
		{
			get_time(time_str);
			printf("[TEMPERATURE-%s]: ", time_str);
			printf("The cpu temperature change too fast: ");
			printf("CPU[%d-%d]\n", board_tp, board_old);
			return -1;			
		}
		else
		{
			cpu_old = cpu_tp;
			board_old = board_tp;
			
			if (second < 4000 || (times % 4000) == 0)
			{
				get_time(time_str);
				printf("[TEMPERATURE-%s]: Read the temperature now: ", time_str);
				printf("CPU-%d, Board-%d\n", cpu_tp, board_tp);
			}
		}

        gettimeofday(&tm_end,NULL);
        diff = ((tm_end.tv_sec -tm_start.tv_sec)*1000000+(tm_end.tv_usec-tm_start.tv_usec))/1000000.0;
        if(diff>time)
        {
            break;
        }
		usleep(interval * 10000);
		times++;
	}

	return 0;
}
