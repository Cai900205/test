/*
 * DataHandler.c
 *
 *  Created on: 2014-11-14
 *      Author: sren
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "fvl_common.h"
#include "HxDataHandler.h"
#include "fvl_tcp.h"
#include "fvl_task.h"
#include "trainBuffer.h"

#include "hxSocket.h"
#include "hxManager.h"

#define LTRACE()										\
  do {													\
	printf("-----%d @ %s----\n", __LINE__, __func__);	\
  } while(0)

int socketPool[24];


#define SENDER_NUM 8
#define TRAIN_BUF_NUM SENDER_NUM

static Train hx_train[TRAIN_BUF_NUM];


int g_BufIndexMap[256] = {
  0,  1,  2,  3,  4,  5,  6,  7, -1, -1, -1, -1, -1, -1, -1, -1, 

  /* 0x10 => 8, 0x13 => 16 */
  8, -1, -1, 16, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 0x20 => 8, 0x23 => 16 */
  9, -1, -1, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 0x30 => 8, 0x33 => 16 */
  10, -1, -1, 18, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 0x40 => 8, 0x43 => 16 */
  11, -1, -1, 19, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 0x50 => 8, 0x53 => 16 */
  12, -1, -1, 20, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 0x60 => 8, 0x63 => 16 */
  13, -1, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 0x70 => 8, 0x73 => 16 */
  14, -1, -1, 22, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 0x80 => 8, 0x83 => 16 */
  15, -1, -1, 23, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 

  /* all -1 */
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
};

void fvl_hex_dump(const char *name, void *buffer, int len)
{
  uint8_t *buf;
  int i, max;
  int start, stop;
  char tmpbuf[24];
  char *tmpstr = tmpbuf;

  start = (long)buffer & 0xf;
  buf   = (uint8_t*)buffer - start;
  stop  = len + start;
  max   = (len + start + 15) & ~0xf;

  printf("Dump buffer '%s':\n", name);
  for(i = 0; i < max; i++) {
	if(i%16 == 0) {
	  printf("%p: ", &buf[i]);
	  memset(tmpbuf, 0, sizeof(tmpbuf));
	  tmpstr = tmpbuf;
	}

	if((i >= start) && (i <= stop)) {
	  printf("%02x", buf[i]);
	  *tmpstr = buf[i];
	  if(buf[i] < 0x20)
		*tmpstr = '.';
	  if (buf[i] > 0x7e)
		*tmpstr = '?';
	} else {
	  printf("  ");
	  *tmpstr = ' ';
	}
	tmpstr ++;

	if(i%4 == 3)
	  printf(" ");

	if(i%8 == 7)
	  printf(" ");

	if(i%16 == 15) {
	  printf("  %s\n", tmpbuf);
	}
  }

  return;
}

int getSocket(uint8_t chanId){
  int index = getChannelBufIndex(chanId);
  if(index == -1 || index>=24) {
	printf("fail to get socket by chanId: chanId=%d", chanId);
	exit(1);
   }else {
	return socketPool[index];
  }
}

struct FrameBuf* getFrame(struct WorkerBuf* pWBuf, int frameId, int chanId)
{
  struct FrameBuf* ret = NULL;
  struct FrameBuf* empty = NULL;
  struct FrameBuf* frame;
  int i;

  for(i = 0; i < WORKER_CHAN_BUFNUM; i++) {
	frame = &pWBuf->buff[i];

	if(frame->frameId == frameId) {
	  ret = frame;
	  break;
	}

	if((empty == NULL) && (frame->totalLines == 0)) {
	  empty = frame;
	}
  }

  if(ret == NULL) {
	if(empty != NULL) {
	  empty->frameId = frameId;
	  empty->chanId  = chanId;
	}
	return empty;
  }

  return ret;
}

void freeFrame(struct FrameBuf* pFBuf)
{
  int i;

  pFBuf->totalLines = 0;
  pFBuf->frameId    = -1;
  pFBuf->chanId     = -1;

  for(i = 0; i < 256; i++) {
	pFBuf->lines[i].header = NULL;
  }

  return;
}

void dumpWorkerBuf(struct WorkerBuf* pWBuf)
{
  struct FrameBuf* pFBuf;
  int i;

  for(i = 0; i < WORKER_CHAN_BUFNUM; i++) {
	pFBuf = &pWBuf->buff[i];
	printf("Worker buff[%d]: totalLines=%u, frameId=%u, chanI%u\n",
		   i, pFBuf->totalLines, pFBuf->frameId, pFBuf->chanId);
  }

  return;
}

struct FrameBuf* saveLine(struct WorkerBuf* pWBuf, struct DataHeader *pHeader)
{
  struct FrameBuf* pFBuf = NULL;
  struct DataLine* pDLine;
  int frameId;
  int chanId;
  uint8_t lineId;

  frameId = pHeader->frameId;
  chanId  = pHeader->chanId;
  pFBuf = getFrame(pWBuf, frameId, chanId);

  if(pFBuf == NULL) {
	printf("GetFrame failed. frameId = %u, chanId = %u\n", frameId, chanId);
	dumpWorkerBuf(pWBuf);
	return NULL;
  }

  lineId = pHeader->lineId;
  pDLine = &pFBuf->lines[lineId];
  if(pDLine->header != NULL) {
	printf("Save line failed. frameId = %u, chanId = %u, lineId = %u\n",
		   frameId, chanId, lineId);
  } else {
	uint32_t len;
	pDLine->header = pHeader;
	len = GETLEN_BY_CHANID(chanId);
	pDLine->dataLength = len;
	memcpy(pDLine->data, pHeader, len);
	pFBuf->totalLines ++;
  }

  return pFBuf;
}

struct FrameBuf* getFullFrame(struct WorkerBuf* pWBuf)
{
  int i;
  struct FrameBuf* frame;
  struct FrameBuf* ret = NULL;

  for(i = 0; i < WORKER_CHAN_BUFNUM; i++) {
	frame = &pWBuf->buff[i];
	if(frame->totalLines >= 256) {
	  ret = frame;
	  break;
	}
  }

  return ret;
}

#pragma pack(1) //设定struct对齐到1字节

typedef struct data_line{
  int32_t head;
  int8_t flag;
  int16_t page_num;
  int8_t line_num;
  int64_t temp_data[6];
  int64_t pic_data[2048];
}data_line_t;

data_line_t* create_line(int8_t flag){
  static int16_t page_num = 0x8888;
  static int8_t line_num = 0x80;
  static int64_t temp_data_f = 0x11111111;
  static int64_t pic_data_f = 0x11111111;

  int i;

  data_line_t* d = (data_line_t*) malloc(sizeof(data_line_t));
  d->head = 0xFAF33400;
  d->flag = flag;
  d->page_num = page_num++;
  d->line_num = line_num++;
  for(i=0; i<6; i++){
	d->temp_data[i] = temp_data_f++;
  }
  for(i=0; i<2048; i++){
	d->pic_data[i] = pic_data_f++;
  }
  return d;
}

typedef struct data_line_dgp{
  int32_t head;
  int8_t flag;
  int16_t page_num;
  int8_t line_num;
  int8_t pre_data[12];
  int8_t temp_data[12];
  int64_t pic_data[768];
}data_line_dgp_t;

data_line_dgp_t* create_dgp_line(int8_t flag){
  static int16_t page_num = 0x8888;
  static int8_t line_num = 0x80;
  static int8_t temp_data_f = 0x11;
  static int64_t pic_data_f = 0x11111111;

  int i;

  data_line_dgp_t* d = (data_line_dgp_t*) malloc(sizeof(data_line_dgp_t));
  d->head = 0xFAF33400;
  d->flag = flag;
  d->page_num = page_num++;
  d->line_num = line_num++;
  for(i=0; i<12; i++){
	d->temp_data[i] = temp_data_f++;
  }
  for(i=0; i<768; i++){
	d->pic_data[i] = pic_data_f++;
  }
  return d;
}


void sender(int seq){
  int sock;
  TrainBox* tb;
  //uint8_t* buf;
  int sended;
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(seq+12, &cpuset);
  pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
  
  sock = getSocket(seq);
  tb = hx_train[seq].reader;

  /* if (0<=seq && seq<=1){ */
  /* 	printf("seq: %d", seq); */
  /* 	data_line_t* dl; */
  /* 	while(1){ */
  /* 	  dl = create_line(1); */
  /* 	  send(sock, dl, sizeof(data_line_t),  0); */
  /* 	  free(dl); */
  /* 	} */
  /* }else if (2<=seq && seq<=6){ */
  /* 	data_line_dgp_t* dm; */
  /* 	while(1){ */
  /* 	  dm = create_dgp_line(2); */
  /* 	  send(sock, dm, sizeof(data_line_dgp_t), 0); */
  /* 	  free(dm); */
  /* 	} */
  /* } */

  while(1){
  	if (tb->filled==0){
  	  usleep(TRAIN_WAIT_USEC);
  	  continue;
  	}
  	sended =  send(sock, tb->buf, tb->filled , 0);
  	tb->filled=0;
  	tb = tb->next;
  }
}

int socketPoolInit(const char* ipAddr, int basePort){
  /* int i; */
  fvl_tcp_socket_t tcp;
  /* in_addr_t ip; */
  /* int port; */
  int rvl;
  int k;

  k = 0;
	
  /* rvl = fvl_tcp_init(&tcp, inet_addr("192.168.1.21"), 5000); */
  /* socketPool[k++]= tcp.sock; */
  rvl = fvl_tcp_init(&tcp, inet_addr("192.168.1.21"), 7000);
  socketPool[k++] = tcp.sock;
  rvl = fvl_tcp_init(&tcp, inet_addr("192.168.1.21"), 7001);
  socketPool[k++] = tcp.sock;
  rvl = fvl_tcp_init(&tcp, inet_addr("192.168.1.21"), 7002);
  socketPool[k++] = tcp.sock;
  rvl = fvl_tcp_init(&tcp, inet_addr("192.168.1.21"), 7003);
  socketPool[k++] = tcp.sock;
  rvl = fvl_tcp_init(&tcp, inet_addr("192.168.1.21"), 7004);
  socketPool[k++] = tcp.sock; 
  rvl = fvl_tcp_init(&tcp, inet_addr("192.168.1.21"), 7005);
  socketPool[k++] = tcp.sock; 
  rvl = fvl_tcp_init(&tcp, inet_addr("192.168.1.21"), 7006);
  socketPool[k++] = tcp.sock; 
  rvl = fvl_tcp_init(&tcp, inet_addr("192.168.1.21"), 7007);
  socketPool[k++] = tcp.sock; 
  rvl = fvl_tcp_init(&tcp, inet_addr("192.168.1.21"), 7008);
  socketPool[k++] = tcp.sock; 

  /* for(i = 0; i < 24; i++) { */
  /*     ip   = inet_addr(ipAddr); */
  /*     port = basePort + i; */

  /*     printf("Will init socket, to %s:%d\n", ipAddr, port); */
  /*     rvl = fvl_tcp_init(&tcp, ip, port); */
  /*     if(rvl < 0) { */
  /*         printf("Init socket#%d error! ip=%s, port=%u", i, ipAddr, port); */
  /*         return -1; */
  /*     } else { */
  /*         printf("Connect to %s:%d successfully\n", ipAddr, port); */
  /*     } */

  /*     socketPool[i] = tcp.sock; */
  /* } */

  return rvl;
}

void revert8B(uint8_t* data){
  uint8_t t;
  int i;
  for (i=0; i<4; i++){
	t = data[i];
	data[i] = data[7-i];
	data[7-i] = t;
  }
}

#define B_LEN 1024*1024*2
static uint32_t last_header = 0;
static uint32_t next_header = 0;


int last_good_frame_num, good_frame_num;
int last_bad_frame_num, bad_frame_num;

uint64_t st_seconds = 0;
uint64_t st_start   = 0;
uint64_t st_errors  = 0;

static inline uint64_t my_ntohll(uint64_t be) 
{
    uint64_t ret;

    ret  = 0;

    ret |= be & 0xff;
    be  >>= 8;

    ret <<= 8;
    ret |= be & 0xff;
    be  >>= 8;

    ret <<= 8;
    ret |= be & 0xff;
    be  >>= 8;

    ret <<= 8;
    ret |= be & 0xff;
    be  >>= 8;

    ret <<= 8;
    ret |= be & 0xff;
    be  >>= 8;

    ret <<= 8;
    ret |= be & 0xff;
    be  >>= 8;

    ret <<= 8;
    ret |= be & 0xff;
    be  >>= 8;

    ret <<= 8;
    ret |= be & 0xff;

    return ret;
}

uint64_t g_mmbuf[1024*1024];

void my_memcpy(uint64_t *dst, uint64_t *src, int cnt)
{
    int i;

    for(i = 0; i < cnt; i++) {
	dst[i] = src[i];
    }
}

uint64_t g_delta = 1;

#define CHECK_STRIPE	16
void check_data_add(uint8_t* buf, int cnt, uint64_t delta)
{
    static uint64_t last_dw = -1;
    static uint64_t s_count = 0;
    uint64_t *p64;
    uint64_t cur;
    int i;

    p64 = (uint64_t *)buf;

    cnt   /= CHECK_STRIPE;
    delta *= CHECK_STRIPE;

//    my_memcpy(g_mmbuf, p64, cnt);

    for(i = 0; i < cnt; i++) {
	//cur  = my_ntohll(p64[i]);
	cur  = p64[i*CHECK_STRIPE];
	if(cur != (last_dw+delta)) {
	    printf("#%lu Error, last=%016lx, cur=%016lx\n", s_count+i*CHECK_STRIPE, last_dw, cur);
	    st_errors += 1;
	}
	last_dw = cur;
    }

    s_count += cnt;
}

#define HX504_FRAME_HEADER	0xfaf3340000000000ull


int hx504_frame_check(uint64_t data_header, uint64_t data_first, uint64_t data_last)
{
    static uint64_t frame_id = 0;
    uint64_t num_head_expected = 1;
    uint64_t num_tail_expected = 2054;
    uint64_t offset;
    int ret = 0;

    num_head_expected += frame_id * 2054;
    num_tail_expected += frame_id * 2054;
    offset = frame_id * 2055;

    /* Frame header not matched */
    if(data_header != HX504_FRAME_HEADER) {
	printf("frame-%lu: frame header error @ global offset %lu, header is %016lx\n",
	       frame_id, offset, data_header);
        ret = -1;
    }

    /* Frame head not matched */
    if(data_first != num_head_expected){
	printf("frame-%lu: first data error @ global offset %lu, data is %016lx, expect %016lx\n",
	       frame_id, offset + 1, data_first, num_head_expected);
        ret = -2;
    }

    /* Frame tail not matched */
    if(data_last != num_tail_expected) {
	printf("frame-%lu: last data error @ global offset %lu, data is %016lx, expect %016lx\n",
	       frame_id, offset + 2054, data_last, num_tail_expected);
        ret = -3;
    }

    frame_id ++;

    return ret;
}

void hx504_check(uint8_t* buf, int cnt, uint64_t delta)
{
    static uint64_t last_header;
    static uint64_t last_first;
    static uint64_t frame_rest = 2055;
    uint64_t *frame= 0;
    uint64_t data_header;
    uint64_t data_first;
    uint64_t data_last;
    int rvl;

    frame = (uint64_t *)buf;
    if(frame_rest != 2055) {
	data_last = frame[frame_rest - 1];

	if(frame_rest == 2054)
	    last_first = frame[0];

	rvl = hx504_frame_check(last_header, last_first, data_last);
	if(rvl < 0) {
	    printf("Can not dump data before and after frame head\n");
	    fvl_hex_dump("Buffer start", frame, 128);

	    if(frame_rest >= 16) {
		fvl_hex_dump("Before last data", frame + frame_rest - 16, 128);
	    }
	    fvl_hex_dump("After last data", frame + frame_rest, 128);
	}

	cnt   -= frame_rest;
	frame += frame_rest;
    }

    while(cnt >= 2055) {
	data_header = frame[0];
	data_first  = frame[1];
	data_last   = frame[2054];

	rvl = hx504_frame_check(data_header, data_first, data_last);
	if(rvl < 0) {

	    if(frame_rest < 16) {
		fvl_hex_dump("Buffer start", frame - frame_rest, 128);
	    } else {
		fvl_hex_dump("Before frame head", frame - 16, 128);
	    }
	    fvl_hex_dump("After frame head", frame, 128);
	    fvl_hex_dump("Before last data", frame + 2055 - 16, 128);

	    if((cnt - 2055) < 16) {
		fvl_hex_dump("Buffer end", frame + cnt - 16, 128);
	    } else {
		fvl_hex_dump("After last data", frame + 2055, 128);
	    }
	}

	cnt   -= 2055;
	frame += 2055;
    }

    if(cnt >= 1) {
	last_header = frame[0];
    }

    if(cnt >= 2) {
	last_first  = frame[1];
    }

    frame_rest = 2055 - cnt;

}

void check_data(uint8_t* buf)
{

 uint32_t* curP = (uint32_t*)buf;

  //find header FAF33400
  while(((uint8_t*)curP)-buf < B_LEN){
	if (*curP == 0x34F3FA || *curP == 0xFAF33400)  {
	  if ((next_header - last_header)!=12360){
		//printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! invalid data len: %d\n", (next_header- last_header));
		bad_frame_num++;
	  }else{
		//printf("get frame ### \n");
		good_frame_num++;
	  }
	  last_header = next_header;
	}
	curP++;
	next_header+=4;
  }
}

void data_input_handler(void* priv, uint8_t* data){
  /* data length is 2M, fpga make it*/
  struct WorkerBuf* pWBuf;
  //Train* t;
  int cpu;
  //int i;
  int sock, sended;

  pWBuf = (struct WorkerBuf*) priv;
  cpu = pWBuf->cpu;
  if (cpu>6){
	return;
  }

  //printf("Current cpu: cpu-%d\n", cpu);
  
/*
  sock = getSocket(cpu);
  sended =  send(sock, data, 2*1024*1024 , 0);
  if (sended == 0 || sended ==-1){
  	printf("send data error: %d\n", sended);
  }
*/
  /* t = &(hx_train[pWBuf->cpu]); */
  /* TrainBox* tbw = t->writer; */
  /* while(tbw->filled>0){ */
  /* 	usleep(TRAIN_WAIT_USEC);  */
  /* } */
  /* tbw->buf = data; */
  /* tbw->filled = 2*1024*1024; */
  /* t->writer = tbw->next; */

  //check_data(data);
  //check_data_add(data, 256*1024, g_delta);
  //hx504_check(data, 256*1024, g_delta);
  
  /* sock = getSocket(pWBuf->cpu); */
  /* if (sock==-1){ */
  /* 	printf("cpu:%d\n", pWBuf->cpu); */
  /* } */
  /* {// revert every 8Byte */
  /* 	int len; */
  /* 	int i; */
  /* 	len = 1024*1024*2; */
  /* 	for (i=0; i< len/8; i++){ */
  /* 	  revert8B(data+i*8); */
  /* 	} */
  /* } */
  
}

void dataInputHandler1(struct WorkerBuf* pWBuf, uint8_t* data)
{
  int sock;
  int rvl;
  int i;
  int len;
  uint32_t flag;
  uint8_t chanId;
  struct DataHeader *pHeader;
  struct FrameBuf   *frame;
  int total_len = FVL_SRIO_DMA_BUFSIZE/FVL_SRIO_CTL_SUBBUF_NUM;

  printf("Calling dataInputHandler: total length: %d \n",total_len);
  if(0)
    {
	  char strbuf[32];
	  snprintf(strbuf, 32, "cpu-%02d recv", pWBuf->cpu);
	  fvl_hex_dump(strbuf, data, 128);
    }

  LTRACE();
  if(pWBuf->line_length != 0) {
	char *dest;

	dest  = pWBuf->last_line.data;
	dest += pWBuf->line_length;
	memcpy(dest, data, pWBuf->line_restlen);

    LTRACE();
	pHeader = (void*) pWBuf->last_line.data;
	flag    = ntohl(pHeader->flag);
	if(DATA_HEADER_FLAG != flag) {
	  printf("Data last line error! Flag = %x\n", flag);
	}

	frame = saveLine(pWBuf, pHeader);
	if(frame == NULL) {
	  printf("Save last line failed\n");
	}

    LTRACE();
	data      += pWBuf->line_restlen;
	total_len -= pWBuf->line_restlen;

	pWBuf->line_length  = 0;
	pWBuf->line_restlen = 0;
  }

  i = 0;
  while(total_len > 0) {

	pHeader = (void*) data;
	chanId  = pHeader->chanId;
	len     = GETLEN_BY_CHANID(chanId);

    //LTRACE();
	flag    = ntohl(pHeader->flag);
	if(DATA_HEADER_FLAG != flag) {
	  printf("Data #%d error! Flag = %x\n", i, flag);
	  continue;
	}

    //LTRACE();
	frame = saveLine(pWBuf, pHeader);
	if(frame == NULL) {
	  printf("Save line #%d failed\n", pHeader->lineId);
	}
	//else {
	//      printf("Save line #%d success\n", pHeader->lineId);
	//  }

	data      += len;
	total_len -= len;

	if((total_len != 0) && (total_len < len)) {
	  LTRACE();
	  //    fvl_hex_dump("Copylast", data, 128);
	  pWBuf->line_length  = total_len;
	  pWBuf->line_restlen = len - total_len;
	  printf("copy to %p, from %p, size %d\n", pWBuf->last_line.data, data, total_len);
	  memcpy(pWBuf->last_line.data, data, total_len);

	  pHeader = (void*) pWBuf->last_line.data;
	  flag    = ntohl(pHeader->flag);
	  if(DATA_HEADER_FLAG != flag) {
		printf("Copy data last line error! Flag = %x\n", flag);
	  }

	  break;
	}

	i++;
  }

  LTRACE();
  do{
    LTRACE();
	frame = getFullFrame(pWBuf);
	if(frame == NULL) {
	  break;
	}

    LTRACE();
	chanId = pHeader->chanId;
	sock = getSocket(chanId); 

    LTRACE();
	for(i = 0; i < 256; i++) {
	  struct DataLine *line;

	  line = &frame->lines[i];
	  pHeader = (void*) line->header;
	  chanId = pHeader->chanId;
	  len = GETLEN_BY_CHANID(chanId);
	  // send buffer here
	  rvl = fvl_tcp_send(sock, (void*)line->data, len);
	  if(rvl < 0) {
		printf("Send failed!\n");
	  }
	}

	printf("send frame #%d done\n", pHeader->frameId);
	freeFrame(frame);
  } while(1);

  return;
}

/* enum { */
/*   MODE_HX_QS, */
/*   MODE_HX_DGP, */
/*   MODE_HX_QMIX, */
/*   MODE_HX_DMIX, */
/*   MODE_HX_BUTT */
/* }; */

/* uint32_t g_frameId[24] = {0}; */
/* uint32_t g_lineId[24]  = {0}; */
/* uint32_t g_chanQsId[8]  = { */
/*   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  */
/* }; */
/* uint32_t g_chanDgpId[16]  = { */
/*   0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,  */
/*   0x13, 0x23, 0x33, 0x43, 0x53, 0x63, 0x73, 0x83,  */
/* }; */
/* uint32_t g_chanQS  = 0; */
/* uint32_t g_chanDGP = 0; */

/* struct FrameBuf initDataFrame = { */
/*   .totalLines = 0, */
/*   .frameId    = 0, */
/*   .chanId     = 0, */
/* }; */
/* struct DataLine *initLastLine; */
/* uint32_t initLineLength = 0; */
/* uint32_t initLineRest   = 0; */
/* uint32_t initLineNum    = 0; */

/* void initDataPattern(uint8_t *data, int mode); */
/* void initDataCopy(uint8_t *data)  */
/* { */
/*   int total_len = FVL_SRIO_DMA_BUFSIZE/FVL_SRIO_CTL_SUBBUF_NUM; */
/*   int i; */

/*   i = initLineNum; */
/*   if(initLastLine != NULL) { */
/* 	char *src; */

/* 	src  = initLastLine->data; */
/* 	src += initLineLength; */
/* 	memcpy(data, src, initLineRest); */

/* 	data      += initLineRest; */
/* 	total_len -= initLineRest; */

/* 	initLastLine   = NULL; */
/* 	initLineLength = 0; */
/* 	initLineRest   = 0; */
/*   } */

/*   if(i == 0) { */
/* 	initDataPattern(data, MODE_HX_QS); */
/*   } */

/*   while(total_len > 0) { */
/* 	int len; */

/* 	len     = initDataFrame.lines[i].dataLength; */
/* 	memcpy(data, initDataFrame.lines[i].data, len); */
/* 	if(0) */
/* 	  { */
/* 		struct DataHeader *phdr; */

/* 		phdr = (void*) data; */
/* 		printf("Copy frame #%d, line #%d, i=%d, total_len=%d, len=%d\n", */
/* 			   phdr->frameId, phdr->lineId, i, total_len, len); */
/* 	  } */

/* 	data      += len; */
/* 	total_len -= len; */

/* 	i ++; */
/* 	i &= 0xff; */

/* 	if(total_len < 30000) { LTRACE(); } */

/* 	if(i == 0) { */
/* 	  initDataPattern(data, MODE_HX_QS); */
/* 	} */

/* 	if(total_len < 30000) { LTRACE(); } */

/* 	if((total_len != 0) && (total_len < len)) { */

/* 	  initLastLine   = &initDataFrame.lines[i]; */
/* 	  initLineLength = total_len; */
/* 	  initLineRest   = len - total_len; */
/* 	  initLineNum    = i; */

/* 	  if(total_len < 30000) {  */
/* 		LTRACE(); */
/* 		printf("Total_len = %d, len = %d, rest = %d\n", total_len, len, initLineRest); */
/* 	  } */

/* 	  memcpy(data, initDataFrame.lines[i].data, initLineLength); */

/* 	  if(total_len < 30000) { LTRACE(); } */
/* 	  break; */
/* 	} */

/*   } */

/* } */

/* void initDataPattern(uint8_t *data, int mode) */
/* { */
/*   int i; */
/*   uint32_t chanId; */

/*   struct DataHeader qs_hdr; */
/*   struct DataHeader dgp_hdr; */
/*   struct DataHeader *src; */
/*   uint32_t len; */

/*   if((mode == MODE_HX_QS) || (mode == MODE_HX_QMIX)) { */
/* 	chanId = g_chanQsId[g_chanQS]; */
/* 	qs_hdr.flag    = htonl(DATA_HEADER_FLAG); */
/* 	qs_hdr.chanId  = chanId; */
/* 	qs_hdr.frameId = g_frameId[chanId]; */
/* 	qs_hdr.lineId  = 0; */
/* 	//        g_chanQS ++; */
/* 	g_frameId[chanId] ++; */
/* 	len = QS_DATA_LEN; */
/* 	src = &qs_hdr; */
/*   } */

/*   if((mode == MODE_HX_DGP) || (mode == MODE_HX_DMIX)) { */
/* 	chanId = g_chanQsId[g_chanDGP]; */
/* 	qs_hdr.flag    = htonl(DATA_HEADER_FLAG); */
/* 	qs_hdr.chanId  = chanId; */
/* 	qs_hdr.frameId = g_frameId[chanId]; */
/* 	qs_hdr.lineId  = 0; */
/* 	//        g_chanDGP ++; */
/* 	g_frameId[chanId] ++; */
/* 	len = DGP_DATA_LEN; */
/* 	src = &dgp_hdr; */
/*   } */

/*   for(i = 0; i < 256; i++) { */
/* 	struct DataHeader *phdr; */

/* 	initDataFrame.lines[i].dataLength = len; */
/* 	phdr = (void*)initDataFrame.lines[i].data; */
/* 	*phdr = *src; */
/* 	src->lineId ++; */
/*   } */
/* } */

int g_dumpcpu = 0;
int g_enchan = 1;
int g_enbist = 0;
int g_sriotype = FVL_SRIO_SWRITE;


#define HX_RECV_LEN     (76 + 512*(2048+16) + 4)
#define HX_RECV_MAGIC   0x499602d2

void data_prepare_handler(void *priv, uint8_t** pbuf, int* send_size){
  *send_size = 256;
}

struct WorkerBuf *allocWorkerBuf(void){
  struct WorkerBuf *pWBuf;

  pWBuf = (void*)malloc(sizeof(*pWBuf));
  if(pWBuf == NULL) {
	printf("Allocate Worker Buffer failed\n");
	return NULL;
  }
  memset(pWBuf, 0, sizeof(*pWBuf));
  return pWBuf;
}

uint16_t g_flag = 0; 

const char *g_option = "t:d:bc";
const char *g_usage  = "Usage: %s [-b] [-c] [-d dump_cpu]\n";

void parse_args(int argc, char *argv[])
{
  int opt;
  char strbuf[128];
  char *pstr;
  int len;
  int rest;

  while ((opt = getopt(argc, argv, g_option)) != -1) {
	switch (opt) {
	case 'b':
	  g_enbist = 1;
	  g_enchan = 0;
	  break;
	case 'c':
	  g_enchan = 1;
	  break;
	case 'd':
	  g_dumpcpu = atoi(optarg);
	  break;
	case 't':
	  g_sriotype = atoi(optarg);
	  break;
	default: /* '?' */
	  fprintf(stderr, g_usage, argv[0]);
	  exit(EXIT_FAILURE);
	}
  }

  pstr = strbuf;
  rest = 127;
  if(g_enbist == 1) {
	len   = snprintf(pstr, rest, "Enable BIST mode\n");
	rest -= len;
	pstr += len;
	g_flag |= FVL_SRIO_FLAG_BIST;
  }

  if(g_enchan == 1) {
	len   = snprintf(pstr, rest, "Enable CHAN mode\n");
	rest -= len;
	pstr += len;
	g_flag |= FVL_SRIO_FLAG_CHAN;
  }

  if(g_dumpcpu != 0) {
	len   = snprintf(pstr, rest, "Dump data at CPU-%d\n", g_dumpcpu);
	rest -= len;
	pstr += len;
  }
  return;
}


void init_trains(){
  const int ICT_trainLength = 8;
  const int ICT_trainBoxBufSize = 2*1024*1024;
  int i;
  
  for(i=0; i< TRAIN_BUF_NUM; i++){
	initTrain( &(hx_train[i]), ICT_trainLength, ICT_trainBoxBufSize);
	printf("init train %d=%p\n", i, &(hx_train[i]));
  }
}



hx_socket sock;
pthread_t manager_thread;

/* start a manager thread to listen on ip:port */
void startManager(char* ip, int port){
  printf(" start netWorker manager: ");
  sock.ip = ip;
  sock.port = port;
  pthread_create(&manager_thread, NULL, hx_manager, &sock);
}

void endian_judge()
{
    int data = 0x12345678ul;
    uint8_t *pch = (uint8_t*)&data;

    printf("endian: data=%x, 1st byte is %02x\n", data, pch[0]);
    return;
}

int main(int argc, char *argv[])
{
  fvl_srio_context_t *psrio;
  fvl_thread_arg_t receive_task_port1[FVL_SRIO_BUFFER_NUMBER];
  fvl_thread_arg_t receive_task_port2[FVL_SRIO_BUFFER_NUMBER];
  int rvl;
  int i;
  pthread_t port1_id[FVL_SRIO_BUFFER_NUMBER];
  pthread_t port2_id[FVL_SRIO_BUFFER_NUMBER];
  pthread_t sender_threads[SENDER_NUM];

  printf("###### check stipe is %d $$$$$$$$$$$$$\n", CHECK_STRIPE);
  endian_judge();
  init_trains();
//  socketPoolInit("192.168.1.1", 5000);
  parse_args(argc, argv);

//  startManager("", 8002);
  /* for(i=0; i<SENDER_NUM;i++){ */
  /* 	pthread_create(&(sender_threads[i]), NULL, sender, i); */
  /* } */
  //pthread_create(&(sender_threads[3]), NULL, sender, 3);
  /* pthread_create(&(sender_threads[3]), NULL, sender, 3); */
  
  /* pthread_create(&(sender_threads[3]), NULL, sender, 3); */

  
  /* just init srio as SWRITE mode */
  rvl = fvl_srio_init(&psrio, g_sriotype);
  if(rvl < 0) {
	FVL_LOG("Srio init failed, return %d\n", rvl);
	return -1;
  }
	
  printf("Ready to create pthreads\n");
  for(i=0;i<FVL_SRIO_BUFFER_NUMBER;i++)
    {
	  struct WorkerBuf* pWBuf;

	  pWBuf = allocWorkerBuf();
	  receive_task_port1[i].psrio = psrio;
	  receive_task_port1[i].port  = 0;
	  receive_task_port1[i].bfnum = i;
	  receive_task_port1[i].cpu   = i+1;
	  receive_task_port1[i].priv  = pWBuf;
	  receive_task_port1[i].stat_bytes = 0;
	  receive_task_port1[i].stat_count = 0;
	  receive_task_port1[i].flag = g_flag;
	  if(pWBuf != NULL) {
		pWBuf->cpu = i+1;
		pWBuf->line_length  = 0;
		pWBuf->line_restlen = 0;
	  }

	  rvl = pthread_create(&port1_id[i], NULL, fvl_srio_recver, &receive_task_port1[i]);
	  if (rvl) {
		printf("Port0 : receive thread failed!\n");
		return -errno;
	  } 

#if 1
	  pWBuf = allocWorkerBuf();
	  receive_task_port2[i].psrio = psrio;
	  receive_task_port2[i].port = 1;
	  receive_task_port2[i].bfnum = i;
	  receive_task_port2[i].cpu = i+1+FVL_SRIO_BUFFER_NUMBER;
	  receive_task_port2[i].priv  = pWBuf;
	  receive_task_port2[i].stat_bytes = 0;
	  receive_task_port2[i].stat_count = 0;
	  receive_task_port2[i].flag = g_flag;
	  if(pWBuf != NULL) {
		pWBuf->cpu = receive_task_port2[i].cpu;
		pWBuf->line_length  = 0;
		pWBuf->line_restlen = 0;
	  }
	  if (i<2){
		rvl = pthread_create(&port2_id[i], NULL,fvl_srio_recver, &receive_task_port2[i]);
	  }
	  if (rvl) {
		printf("Port1: receive thread failed!\n");
		return -errno;
	  } 
#endif
    }

  {
#define TIME_ELAPSE_SEC 2
#define TIME_ELAPSE_MS  (TIME_ELAPSE_SEC*1000)
	struct timeval tm_start,tm_end;
	uint64_t frame_count_last[8] = {0};
	uint64_t frame_bytes_last[8] = {0};
	uint64_t frame_count_cur[8] = {0};
	uint64_t frame_bytes_cur[8] = {0};
	uint64_t elapse_ms;
	float perf_p0[4];
	float perf_p1[4];
	float perf_send;
	float st_elapse;

	for(i = 0; i < 4; i++) {
	  frame_count_last[i]   = receive_task_port1[i].stat_count;
	  frame_count_last[i+4] = receive_task_port2[i].stat_count;

	  frame_bytes_last[i]   = frame_count_last[i] * 2*1024*1024;
	  frame_bytes_last[i+4] = frame_count_last[i+4] * 2*1024*1024;
	}

	gettimeofday(&tm_start, NULL);
	st_start = tm_start.tv_sec;
	while(1) {
	  gettimeofday(&tm_end, NULL);
	  elapse_ms = 1000*(tm_end.tv_sec - tm_start.tv_sec)
		+ (tm_end.tv_usec - tm_start.tv_usec)/1000;
	  st_seconds = tm_end.tv_sec - st_start;

	  if(elapse_ms < TIME_ELAPSE_MS) {
		continue;
	  }

	  tm_start = tm_end;

	  for(i = 0; i < 4; i++) {
		frame_count_cur[i]   = receive_task_port1[i].stat_count;
		frame_count_cur[i+4] = receive_task_port2[i].stat_count;

		frame_bytes_cur[i]   = frame_count_cur[i] * 2*1024*1024;
		frame_bytes_cur[i+4] = frame_count_cur[i+4] * 2*1024*1024;
	  }

	  perf_p0[0] = (frame_bytes_cur[0] - frame_bytes_last[0])/(TIME_ELAPSE_SEC*1024*1024.);
	  perf_p0[1] = (frame_bytes_cur[1] - frame_bytes_last[1])/(TIME_ELAPSE_SEC*1024*1024.);
	  perf_p0[2] = (frame_bytes_cur[2] - frame_bytes_last[2])/(TIME_ELAPSE_SEC*1024*1024.);
	  perf_p0[3] = (frame_bytes_cur[3] - frame_bytes_last[3])/(TIME_ELAPSE_SEC*1024*1024.);

	  perf_p1[0] = (frame_bytes_cur[4] - frame_bytes_last[4])/(TIME_ELAPSE_SEC*1024*1024.);
	  perf_p1[1] = (frame_bytes_cur[5] - frame_bytes_last[5])/(TIME_ELAPSE_SEC*1024*1024.);
	  perf_p1[2] = (frame_bytes_cur[6] - frame_bytes_last[6])/(TIME_ELAPSE_SEC*1024*1024.);
	  perf_p1[3] = (frame_bytes_cur[7] - frame_bytes_last[7])/(TIME_ELAPSE_SEC*1024*1024.);

	  //printf("Perf(MiBps): \t%.1f \t%.1f \t%.1f \t%.1f  \t%.1f \t%.1f \t%.1f \t%.1f  get %d frames, %d errors\n",
	  //		 perf_p0[0], perf_p0[1], perf_p0[2], perf_p0[3], perf_p1[0], perf_p1[1], perf_p1[2], perf_p1[3], good_frame_num-last_good_frame_num, bad_frame_num-last_bad_frame_num);
	  st_elapse = st_seconds/3600.0;
	  printf("Perf(MiBps): \t%.1f \t%.1f \t%.1f \t%.1f  \t%.1f \t%.1f \t%.1f \t%.1f  Runing %.2f hours, %d errors\n",
			 perf_p0[0], perf_p0[1], perf_p0[2], perf_p0[3], perf_p1[0], perf_p1[1], perf_p1[2], perf_p1[3], st_elapse, st_errors);
	  last_good_frame_num= good_frame_num;
	  last_bad_frame_num = bad_frame_num;

	  for(i = 0; i < 8; i++) {
		frame_bytes_last[i]   = frame_bytes_cur[i];
	  }
	}
  }

  
  pthread_join(manager_thread, NULL);
  for(i=0;i<FVL_SRIO_BUFFER_NUMBER;i++)
    {
	  pthread_join(port1_id[i], NULL);
	  pthread_join(port2_id[i], NULL);
    }
  for (i=0; i<SENDER_NUM; i++){
  	pthread_join(sender_threads[i], NULL);
	
  }
  return 0;
}

