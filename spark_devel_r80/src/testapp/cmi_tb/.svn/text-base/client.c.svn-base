#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <assert.h>
#include "cmi/cmi_intf.h"
 
#define PORT 1234
#define MAXDATASIZE 100

int test_cmd(int sockfd, int cmd_type, struct sockaddr_in* server)
{
    cmi_cmd_t cmd;
    struct sockaddr_in peer;
    char buf[MAXDATASIZE];

    fprintf(stderr, "\n>>>>>> test message-cmd: type=%s", cmi_desc_cmdtype2str(cmd_type));

    memset(&cmd, 0, sizeof(cmd));
    cmi_msg_hdr_t* hdr = (cmi_msg_hdr_t*)&cmd;
    hdr->sync_tag = 0x5a5a;
    hdr->msg_len = sizeof(cmd);
    hdr->msg_code = msg_code_cmd;
    cmd.cmd_type = cmd_type;

    sendto(sockfd, &cmd, sizeof(cmd), 0, (struct sockaddr *)server, sizeof(*server));

    socklen_t  addrlen;
    addrlen = sizeof(*server);
    ssize_t num;
    while(1) {
        num = recvfrom(sockfd, buf, MAXDATASIZE, 0, (struct sockaddr *)&peer, &addrlen);
        if (num > 0) {
            cmi_msg_dump(ZLOG_LEVEL_NOTICE, buf, num);
            break;
        }
    }
    return(0);
}

int main(int argc, char *argv[])
{
    int sockfd;
    int ret;

    struct hostent *he;
    struct sockaddr_in server;
 
    zlog_init("./zlog.conf");
    ret = cmi_module_init(NULL);
    assert(!ret);

    he = gethostbyname("127.0.0.1"); 
    if ((sockfd=socket(AF_INET, SOCK_DGRAM,0))==-1)
    {
       printf("socket() error\n");
       exit(1);
    }
 
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr= *((struct in_addr *)he->h_addr);

    for (int i=cmd_type_inquiry; i<cmd_type_max; i++) {
        test_cmd(sockfd, i, &server);
    }

    close(sockfd);
}
