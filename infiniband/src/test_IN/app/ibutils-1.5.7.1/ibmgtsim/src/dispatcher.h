/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef IBMS_WORKER_H
#define IBMS_WORKER_H

/****h* IBMS/Worker
* NAME
*	IB Management Simulator MAD Dispatcher: Worker Threads and MAD Queue
*
* DESCRIPTION
*	The simulator stores incoming mads in a special queue that provides
*  randomization of transport time. A group of worker threads is responsible
*  to pop mad messages from the queue, route them to the destination nodes
*  and call their mad processor.
*
* AUTHOR
*	Eitan Zahavi, Mellanox
*
*********/

#include "simmsg.h"
#include <map>
#include <list>
#include <pthread.h>

class IBMSDispatcher {

  struct madItem {
    class IBMSNode *pFromNode; /* the node the mad was injected from */
    uint8_t fromPort;          /* the port number the mad was injected from */
    ibms_mad_msg_t  madMsg;    /* the mad message */
  };

  typedef std::multimap<uint64_t, struct madItem > mmap_uint64_mad;
  typedef std::list< struct madItem > mad_list;

  /* we track our worker threads and timer in the array of sub-threads */
  pthread_t *threads;

  /* the queue of mads waiting for processing */
  mmap_uint64_mad madQueueByWakeup;

  /* lock to synchronize popping up and pushing into the madQueueByWakeup */
  pthread_mutex_t madQueueByWakeupLock;

  /* list of mads waiting for dispatching */
  mad_list madDispatchQueue;

  /* lock to synchronize popping and pushing into mad dispatch list */
  pthread_mutex_t madDispatchQueueLock;

  /* signal the timer waits on - signaled when new mads are pushed into Q */
  pthread_cond_t newMadIntoWaitQ;

  /* signal the workers when new MAD moved to dispatch Q */
  pthread_cond_t newMadIntoDispatchQ;

  /* flag to tell the threads to exit */
  boolean_t exit_now;

  /* average delay from introducing the mad to when it appear on the queue */
  uint64_t avgDelay_usec;

  /* deviation on the delay */
  uint64_t stdDevDelay_usec;

  /* route the mad to the destination by direct route */
  int routeMadToDestByDR(madItem &item);

  /* route the mad to the destination by dest lid */
  int routeMadToDestByLid(madItem &item);

  /* route a mad to the destination node. On the way can drop mads by
     statistics and update the relevant port counters on the actual node. */
  int routeMadToDest(madItem &item);

  /* The callback function for the threads */
  static void *workerCallback(void *context);

  /*
	* The timer thread main - should signal the threads
	* if there is an outstanding mad - or wait for next one
	*/
  static void *timerCallback(void *context);

 public:
  /* constructor */
  IBMSDispatcher(int numWorkers,
                 uint64_t delayAvg_usec, uint64_t delayStdDev_usec);

  ~IBMSDispatcher();

  /* sets the average delay for a mad on the wire */
  int	setDelayAvg(uint64_t delayAvg_usec);

  /* sets the deviation of the delay for a mad on the wire */
  int	setDelayStdDev(uint64_t delayStdDev_usec);

  /* introduce a new mad to the dispatcher */
  int dispatchMad(IBMSNode *pFromNode, uint8_t fromPort, ibms_mad_msg_t &msg);

};

#endif /* IBMS_WORKER_H */
