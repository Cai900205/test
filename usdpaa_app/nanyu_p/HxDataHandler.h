/*
 * DataHandler.h
 *
 *  Created on: 2014-11-14
 *      Author: sren
 */

#ifndef DATAHANDLER_H_
#define DATAHANDLER_H_

#include <inttypes.h>


#define QS_DATA_LEN (8+48+4096*4)
#define DGP_DATA_LEN (8+12+12+3072*2)

#define GETLEN_BY_CHANID(id)    ((id)<8 ? QS_DATA_LEN : DGP_DATA_LEN)

#define DATA_HEADER_FLAG        0xFAF33400

struct DataHeader{
    uint32_t flag; 		//0xFAF33400
    uint8_t chanId;
    /* chanId:
       0x00--0x07: QS1--QS8
       0x10, 0x13: DGP1
       0x20, 0x23: DGP2
       ...
       0x80, 0x83: DGP8
     */
    uint16_t frameId;
    uint8_t lineId;
};

struct DataLine{
    uint32_t lineId;
    int dataLength;
    struct DataHeader *header;
    char data[16384+48+8];
};


struct FrameBuf{
    int totalLines;
    int frameId;
    int chanId;
    int reserve;
    struct DataLine lines[256];
};

#define WORKER_CHAN_BUFNUM 4

struct WorkerBuf {
    struct FrameBuf buff[WORKER_CHAN_BUFNUM];
    struct DataLine last_line;
    int line_length;
    int line_restlen;
    int cpu;
};


extern int g_BufIndexMap[];

static inline int getChannelBufIndex(uint8_t chanId){
    return g_BufIndexMap[chanId];
}


#endif /* DATAHANDLER_H_ */

