
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "hxSocket.h"


#ifndef HXMANAGER_H_
#define HXMANAGER_H_

/*
  start a manager thread, the thread will accept connection and start a new thread to talk with adminner.
*/
#define HX_MANAGER 1

void* hx_manager(void* p);


#endif /* HXMANAGER_H_ */
