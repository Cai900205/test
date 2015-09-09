#include <stdio.h>
#include <time.h>
#include <string.h>

void get_time(char *str_t)
{
	time_t now;
	char str[30];
	memset(str,0,sizeof(str));
	time(&now);
	strftime(str,30,"%Y-%m-%d %H:%M:%S",localtime(&now));
	int len = strlen(str);
	str[len] = '\0';
	strcpy(str_t,str);
}

