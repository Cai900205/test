#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
void usage()
{
    printf("[option]:  --time       Test time for this program.\n");
    printf("           --passes     Test times for this program.\n");
    printf("           --workers    Worker numbers of this program.(no use)\n");
    printf("           --bind       This program is or not bind.(no use)\n");
    printf("           --interval   Seconds to sleep between two times of test.\n");
    printf("           --help       The help of this program.\n");
    printf("           --version    The version of this program.\n");
}
