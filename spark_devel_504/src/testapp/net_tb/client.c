#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdint.h>
#include <assert.h>

#include "spark.h"
#include "net/net_intf.h"
#include "zlog/zlog.h"

#define MAX_BUFSIZE (0x10000)
      
int main(int argc, char **argv)
{
  struct net_handle* file_ctx = NULL;
  struct timeval start,end;

  char rcv_buf[MAX_BUFSIZE];
  if (argc < 3) {
    printf("usage:need argument ip_address port_num\n");
    exit(-1);
  }
  int rvl=0; 
  int port_num= strtoul(argv[2],NULL,0);
  zlog_init("./zlog.conf");
  rvl = net_module_init(NULL);
  assert(!rvl);

  file_ctx = net_open(argv[1],port_num,4,16,SPK_DIR_WRITE,net_intf_tcp);
  if (!file_ctx) {
    printf("failed to open net\n");
    rvl = SPKERR_BADRES;
    goto out;
  }
  rvl= net_intf_is_connected(file_ctx);
  if(!rvl) {
    printf("net is not connected!\n");
	goto out;
  }
  
  uint64_t xfer=0;
  gettimeofday(&start, NULL);
  uint64_t total_count=0;
  while (1) {

    xfer = net_write(file_ctx, rcv_buf, MAX_BUFSIZE);
    if(xfer != MAX_BUFSIZE) {
	    printf("send data error=>xfer:%ld\n",xfer);
	} else {
//	    printf("write OK!\n");
	}

	total_count++;
    gettimeofday(&end, NULL);
    float diff=(end.tv_sec-start.tv_sec)+(end.tv_usec - start.tv_usec)/1000000.0;
	if(diff > 5) {
    	float speed=(total_count*MAX_BUFSIZE)/1024/diff;
		printf("time:%-15f s speed:%-15f KB/s\n",diff,speed);
        gettimeofday(&start, NULL);
		total_count=0;
	}
  }

out:
    if (file_ctx) {
        net_close(file_ctx);
        file_ctx = NULL;
    }
  return rvl;
}
