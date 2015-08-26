
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>


#define TRAIN_WAIT_USEC 1
/** usage:

  Train* t = malloc(sizeof(Train));
  initTrain(t, 3, 10);
  // init a train with 3 box, each box buffer size is 10Byte
  
writer:
  t->writer->buf = "abc";
  t->writer->filled = 3; //3 is size of buf filled
  t->writer = t->writer->next;
  
reader:
  char* data = t->reader->buf;
  t->reader->filled = 0;
  t->reader = t->reader->next;

 */


typedef struct TrainBox{
  uint8_t* buf;
  struct TrainBox* next;
  int filled;
  int size;
}TrainBox;

typedef struct Train{
  int trainLen;
  int boxbuf_size;

  int reader_num;
  int writer_num;

  struct TrainBox* writer;
  struct TrainBox* reader;
}Train;

void initTrain(Train* t, int trainLen, int boxbuf_size);

int writeTrain(Train* t, uint8_t* data, int size);
int readTrain(Train* t, uint8_t* data, int* size);

void printTrain(Train* t);
