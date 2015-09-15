#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <time.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#ifdef HAVE_SYS_FILE_H
#  include <sys/file.h>
#endif
#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#if defined (HAVE_STRING_H)
#  include <string.h>
#else /* !HAVE_STRING_H */
#  include <strings.h>
#endif /* !HAVE_STRING_H */

#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif

#include <time.h>

#ifdef READLINE_LIBRARY
#  include "readline.h"
#  include "history.h"
#else
#  include <readline/readline.h>
#  include <readline/history.h>
#endif

#define IDT_CMD_NUM 7

extern char *xmalloc PARAMS((size_t));

typedef struct CPS_OP
{
    char *name;
    int (*fp)();
}cps_op_t;

void get_cmd(char **p);
int split_string(char *s,char _cmd[][100]);
void print_array(char _arr[][100],int _len);
void CPS1432_Routingtable(int fd);
int IDT_Read(int fd,uint32_t offset,uint32_t *read_buffer);
void cmd_translate(char cli_argv[][100],int len,int fd);

int CPS_Help();
int CPS_Read(int fd,uint32_t offset);
int CPS_Write(int fd,uint32_t offset,uint32_t data);
int CPS_Route(int fd,uint32_t src_port,uint32_t dst_port,uint32_t dst_id);
int CPS_Init(int fd);
int CPS_Print(int fd,int port_num);
int CPS_Recovery(int fd,int i);
