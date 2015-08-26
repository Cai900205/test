
#ifndef __FVL_COMMON_H__
#define __FVL_COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include <error.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include "fvl_srio.h"

#define FVL_LOG(fmt...)         printf(fmt)

void data_input_handler(void* priv, uint8_t* data);
void data_prepare_handler(void *priv, uint8_t** pbuf, int* send_size);

#endif // __FVL_COMMON_H__

