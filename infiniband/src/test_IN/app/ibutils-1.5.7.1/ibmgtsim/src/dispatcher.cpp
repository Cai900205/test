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

#include "dispatcher.h"
#include "server.h"
#include "msgmgr.h"
#include <math.h>

//////////////////////////////////////////////////////////////
//
// CLASS  IBMSDispatcher
//

/* constructor */
IBMSDispatcher::IBMSDispatcher(
  int numWorkers,
  uint64_t dAvg_usec,
  uint64_t dStdDev_usec)
{
  MSG_ENTER_FUNC;

  MSGREG(err1, 'E', "Failed to init timer thread.", "dispatcher");
  MSGREG(err2, 'E', "Failed to init worker thread:$.", "dispatcher");

  avgDelay_usec = dAvg_usec;
  stdDevDelay_usec = dStdDev_usec;

  /* init the locks */
  pthread_mutex_init( &madQueueByWakeupLock, NULL);
  pthread_mutex_init( &madDispatchQueueLock, NULL);

  /* init signals */
  pthread_cond_init( &newMadIntoWaitQ, NULL);
  pthread_cond_init( &newMadIntoDispatchQ, NULL);

  /* we will need numWorkers + 1 threads */
  threads = (pthread_t*)calloc(sizeof(pthread_t), numWorkers+1);

  /* initialize the exit mode */
  exit_now = FALSE;

  /* construct and init the thread that implements the timer  */
  if (pthread_create(&threads[0], NULL, &IBMSDispatcher::timerCallback, this))
  {
	  MSGSND(err1);
	  exit(1);
  }

  /* construct and init the worker threads */
  for (int i = 1; i <= numWorkers; i++)
  {
	  if (pthread_create(&threads[i],
								NULL, &IBMSDispatcher::workerCallback, this))
	  {
		  MSGSND(err2, i);
		  exit(1);
	  }
  }

  MSG_EXIT_FUNC;
}

/* distructor */
IBMSDispatcher::~IBMSDispatcher()
{
  MSG_ENTER_FUNC;

  exit_now = TRUE;

  /* first tell the timer to exit */
  pthread_mutex_lock( &madQueueByWakeupLock );
  pthread_cond_signal( &newMadIntoWaitQ );
  pthread_mutex_unlock( &madQueueByWakeupLock );

  /* now broadcast to all worker threads */
  pthread_mutex_lock( &madDispatchQueueLock );
  pthread_cond_broadcast( &newMadIntoDispatchQ );
  pthread_mutex_unlock( &madDispatchQueueLock );

  pthread_mutex_destroy( &madQueueByWakeupLock );
  pthread_mutex_destroy( &madDispatchQueueLock );
  pthread_cond_destroy( &newMadIntoWaitQ );
  pthread_cond_destroy( &newMadIntoDispatchQ );

  MSG_EXIT_FUNC;
}

/* sets the average delay for a mad on the wire */
int IBMSDispatcher::setDelayAvg(uint64_t dAvg_usec)
{
  avgDelay_usec = dAvg_usec;
  return 0;
}

/* sets the deviation of the delay for a mad on the wire */
int IBMSDispatcher::setDelayStdDev(uint64_t dStdDev_usec)
{
  stdDevDelay_usec = dStdDev_usec;
  return 0;
}

/* introduce a new mad to the dispatcher */
int IBMSDispatcher::dispatchMad(
  IBMSNode *pFromNode,
  uint8_t fromPort,
  ibms_mad_msg_t &msg)
{
  MSG_ENTER_FUNC;

  MSGREG(inf1, 'V',
         "Queued a mad from:$ tid:$ to expire in $ msec $ usec at $ usec",
         "dispatcher");

  /* randomize the time we want the mad wait the event wheel */
  uint64_t waitTime_usec =
    llrint((2.0 * rand()) / RAND_MAX * stdDevDelay_usec) +
	  (avgDelay_usec - stdDevDelay_usec);

  madItem item;
  item.pFromNode = pFromNode;
  item.fromPort = fromPort;
  item.madMsg = msg;

  struct timeval now;
  gettimeofday(&now, NULL);
  uint64_t wakeupTime_up = now.tv_sec*1000000 + now.tv_usec + waitTime_usec;

  /* set the timer to the next event - trim to max delay of the current mad */
  uint32_t waitTime_msec = waitTime_usec/1000;

  MSGSND(inf1, item.pFromNode->getIBNode()->name,
         msg.header.trans_id,
         waitTime_msec, waitTime_usec, wakeupTime_up);

  /* obtain a lock on the Q */
  pthread_mutex_lock( &madQueueByWakeupLock );

  /* store the mad in the sorted by wakeup map */
  madQueueByWakeup.insert(pair< uint64_t, madItem>(wakeupTime_up, item));

  /* signal the timer */
  pthread_cond_signal( &newMadIntoWaitQ );

  /* release the lock */
  pthread_mutex_unlock( &madQueueByWakeupLock );

  MSG_EXIT_FUNC;
  return 0;
}

/*
  The call back function for the threads
  Loop to handle all outstanding MADs (those expired their wakeup time)
*/
void *
IBMSDispatcher::workerCallback(void *context)
{
  MSG_ENTER_FUNC;
  IBMSDispatcher *pDisp = (IBMSDispatcher *)context;

  MSGREG(inf1,'V',"Entered workerCallback","dispatcher");

  MSGSND(inf1);

  madItem curMadMsgItem;

  /* get the first message in the waiting map */
  pthread_mutex_lock( &pDisp->madDispatchQueueLock );

  while (! pDisp->exit_now)
  {
    if (! pDisp->madDispatchQueue.empty() )
	 {
		 curMadMsgItem = pDisp->madDispatchQueue.front();
		 pDisp->madDispatchQueue.pop_front();
		 pthread_mutex_unlock( &pDisp->madDispatchQueueLock );

		 pDisp->routeMadToDest( curMadMsgItem );

		 pthread_mutex_lock( &pDisp->madDispatchQueueLock );
	 }
	 else
	 {
		 pthread_cond_wait( &pDisp->newMadIntoDispatchQ,
								  &pDisp->madDispatchQueueLock );
	 }
  }

  pthread_mutex_unlock( &pDisp->madDispatchQueueLock );
  MSG_EXIT_FUNC;
  return NULL;
}

/*
   The the timer thread main
	Loop using a cond wait -
	When loop expires check to see if work exists and signal the threads
	Then sleep for next time
*/
void *
IBMSDispatcher::timerCallback(void *context)
{
  MSG_ENTER_FUNC;
  IBMSDispatcher *pDisp = (IBMSDispatcher *)context;
  struct timespec nextWakeup;
  struct timeval now;
  madItem curMadMsgItem;
  boolean_t wait;

  MSGREG(inf1, 'V', "Schedule next timer callback in $ [msec]", "dispatcher");
  MSGREG(inf2, 'V', "Signaling worker threads", "dispatcher");

  /* obtain a lock on the Q */
  pthread_mutex_lock( &pDisp->madQueueByWakeupLock );

  while (! pDisp->exit_now ) {

	  gettimeofday(&now, NULL);
	  mmap_uint64_mad::iterator mI = pDisp->madQueueByWakeup.begin();
	  if (mI != pDisp->madQueueByWakeup.end())
	  {
		  uint64_t curTime_usec = now.tv_sec*1000000 + now.tv_usec;
		  uint64_t wakeUpTime_usec = (*mI).first;

		  /* we are looking for an entry further down the road */
		  if (curTime_usec < wakeUpTime_usec)
		  {
			  /* just calculate the next wait time */
			  nextWakeup.tv_sec = wakeUpTime_usec / 1000000;
			  nextWakeup.tv_nsec = 1000*(wakeUpTime_usec %1000000);
			  MSGSND(inf1, (wakeUpTime_usec - curTime_usec) / 1000 );
			  wait = TRUE;
		  }
		  else
		  {
			  /* pop the message and move to the dispatch queue */
			  curMadMsgItem = (*mI).second;
			  MSGSND(inf2);
			  pthread_mutex_lock( &pDisp->madDispatchQueueLock );
			  pDisp->madDispatchQueue.push_back( curMadMsgItem );
			  pDisp->madQueueByWakeup.erase(mI);
			  pthread_cond_signal( &pDisp->newMadIntoDispatchQ );
			  pthread_mutex_unlock( &pDisp->madDispatchQueueLock );
			  wait = FALSE;
		  }
	  } else {
		  nextWakeup.tv_sec = now.tv_sec+2;
		  nextWakeup.tv_nsec = 0;
		  wait = TRUE;
		  MSGSND(inf1, 1000);
	  }

	  if ( wait == TRUE )
	  {
		  pthread_cond_timedwait( &pDisp->newMadIntoWaitQ,
										  &pDisp->madQueueByWakeupLock,
										  &nextWakeup );
	  }
  }

  pthread_mutex_unlock( &pDisp->madQueueByWakeupLock );

  MSG_EXIT_FUNC;
  return NULL;
}

/* do LID routing */
int
IBMSDispatcher::routeMadToDestByLid(
  madItem &item)
{
  MSG_ENTER_FUNC;
  IBMSNode *pCurNode = NULL;
  IBMSNode *pRemNode = item.pFromNode;
  IBPort   *pRemIBPort; /* stores the incoming remote port */
  uint16_t lid = item.madMsg.addr.dlid;
  uint8_t   prevPortNum = 0;
  int hops = 0;

  MSGREG(inf0, 'I', "Routing MAD mgmt_class:$ method:$ tid:$ to lid:$ from:$ port:$", "dispatcher");
  MSGREG(inf1, 'E', "Got to dead-end routing to lid:$ at node:$ (fdb)",
         "dispatcher");
  MSGREG(inf2, 'I', "Arrived at lid $ = node $ after $ hops", "dispatcher");
  MSGREG(inf3, 'E', "Got to dead-end routing to lid:$ at node:$ port:$",
         "dispatcher");
  MSGREG(inf4, 'E', "Got to dead-end routing to lid:$ at HCA node:$ port:$ lid:$",
         "dispatcher");
  MSGREG(inf5, 'V', "Got node:$ through port:$", "dispatcher");

  MSGSND(inf0,
	 item.madMsg.header.mgmt_class,
	 item.madMsg.header.method,
	 item.madMsg.header.trans_id,
    lid,
	 item.pFromNode->getIBNode()->name,
	 item.fromPort);

  int isVl15 = (item.madMsg.header.mgmt_class == IB_MCLASS_SUBN_LID);

  prevPortNum = item.fromPort;

  /* we will stop when we are done or stuck */
  while (pRemNode && (pCurNode != pRemNode))
  {
    /* take the step */
    pCurNode = pRemNode;

    /* this sim node function is handling both HCA and SW under lock ... */
    if (pCurNode->getIBNode()->type == IB_CA_NODE)
    {
      /* HCA node - we are either done or get out from the client port num */
      if (hops == 0)
      {
        // catch cases where the lid is our own lid - use the port info for that
        if (cl_ntoh16(pCurNode->nodePortsInfo[item.fromPort].base_lid) == lid)
        {
          pRemNode = pCurNode;
          pRemIBPort = pCurNode->getIBNode()->getPort(item.fromPort);
        }
        else
        {
          if (pCurNode->getRemoteNodeByOutPort(
                item.fromPort, &pRemNode, &pRemIBPort, isVl15))
          {
            MSGSND(inf3, lid, pCurNode->getIBNode()->name, item.fromPort);
            MSG_EXIT_FUNC;
            return 1;
          }
          if (pRemIBPort)
          {
            MSGSND(inf5, pRemNode->getIBNode()->name, pRemIBPort->num);
            prevPortNum = pRemIBPort->num;
          }
        }
      }
      else
      {
        /* we mark the fact we are done */
        pRemNode = pCurNode;
      }
    }
    else
    {
      /* Switch node */
      if (pCurNode->getRemoteNodeByLid(lid, &pRemNode, &pRemIBPort, isVl15))
      {
        MSGSND(inf1, lid, pCurNode->getIBNode()->name);
        MSG_EXIT_FUNC;
        return 1;
      }
      /* if the remote port identical to cur node that target is this switch */
      if (pCurNode == pRemNode)
      {
        MSGSND(inf2, lid, pRemNode->getIBNode()->name, hops);
        int res = pRemNode->processMad(prevPortNum, item.madMsg);
        MSG_EXIT_FUNC;
        return(res);
      }
      if (pRemIBPort)
      {
        MSGSND(inf5, pRemNode->getIBNode()->name, pRemIBPort->num);
        prevPortNum = pRemIBPort->num;
      }
    }
    hops++;
  }

  /* validate we reached the target node */
  if (! pRemNode) return(1);
  if (! pRemIBPort) return(1);

  /* check the lid of the target port we reach - it must match target */
  /* TODO: Support LMC in checking if LID routing target reached */
  if (lid == cl_ntoh16(pRemNode->nodePortsInfo[pRemIBPort->num].base_lid))
  {
    MSGSND(inf2, lid, pRemNode->getIBNode()->name, hops);
    int res = pRemNode->processMad(pRemIBPort->num, item.madMsg);
    MSG_EXIT_FUNC;
    return(res);
  }
  else
  {
    /* we did not get to the target */
    MSGSND(inf4, lid, pRemNode->getIBNode()->name, pRemIBPort->num,
           cl_ntoh16(pRemNode->nodePortsInfo[pRemIBPort->num].base_lid));
    MSG_EXIT_FUNC;
    return 1;
  }
  MSG_EXIT_FUNC;
}

/* do Direct Routing */
int
IBMSDispatcher::routeMadToDestByDR(
  madItem &item)
{
  MSG_ENTER_FUNC;
  IBMSNode *pCurNode = NULL;
  IBMSNode *pRemNode = item.pFromNode;
  IBPort   *pRemIBPort = NULL; /* stores the incoming remote port */
  uint8_t   inPortNum = item.fromPort;
  int hops = 0;         /* just for debug */

  /* we deal only with SMP with DR sections */
  ib_smp_t *p_mad = (ib_smp_t *)(&(item.madMsg.header));

  MSGREG(inf0, 'I', "Routing MAD tid:$ by DR", "dispatcher");
  MSGREG(inf1, 'I', "Got to dead-end routing by MAD tid:$ at node:$ hop:$",
         "dispatcher");
  MSGREG(inf2, 'I', "MAD tid:$ to node:$ after $ hops", "dispatcher");
  MSGREG(err1, 'E', "Combination of direct and lid route is not supported by the simulator!", "dispatcher");

  MSGSND(inf0, cl_ntoh64(item.madMsg.header.trans_id));

  /* check that no dr_dlid or drdlid are set */
  if ((p_mad->dr_slid != 0xffff) || (p_mad->dr_slid != 0xffff) )
  {
    MSGSND(err1);
    MSG_EXIT_FUNC;
    return(1);
  }

  /* the direction of the hop pointer dec / inc is by the return bit */
  if (ib_smp_is_d(p_mad))
  {
    MSGREG(inf1, 'V', "hop pointer is $ and hop count is $ !", "dispatcher");
    MSGSND(inf1, p_mad->hop_ptr, p_mad->hop_count);

    // TODO implement direct route return algorithm
    p_mad->hop_ptr--;

    while(p_mad->hop_ptr > 0)
    {
      pCurNode = pRemNode;
      hops++;
      MSGREG(inf2, 'V', "hops is $", "dispatcher");
      MSGSND(inf2, hops);

      if (pCurNode->getRemoteNodeByOutPort(
            p_mad->return_path[p_mad->hop_ptr--], &pRemNode, &pRemIBPort, 1))
      {
        MSGSND(inf1, cl_ntoh64(p_mad->trans_id),
               pCurNode->getIBNode()->name, hops);
        MSG_EXIT_FUNC;
        return 1;
      }
    }
  }
  else
  {
    /* travel out the path - updating return path port num */

    /* we should start with 1 (init should be to zero) */
    p_mad->hop_ptr++;

    while(p_mad->hop_ptr <= p_mad->hop_count)
    {
      pCurNode = pRemNode;
      hops++;

      if (pCurNode->getRemoteNodeByOutPort(
            p_mad->initial_path[p_mad->hop_ptr], &pRemNode, &pRemIBPort, 1))
      {
        MSGSND(inf1, cl_ntoh64(p_mad->trans_id),
               pCurNode->getIBNode()->name, hops);
        MSG_EXIT_FUNC;
        return 1;
      }

      /* update the return path */
      p_mad->return_path[p_mad->hop_ptr] = pRemIBPort->num;

      p_mad->hop_ptr++;
    }
  }

  /* validate we reached the target node */
  if (! pRemNode) return(1);
  //if (! pRemIBPort) return(1);
  if (pRemIBPort) inPortNum = pRemIBPort->num;
  MSGSND(inf2, cl_ntoh64(p_mad->trans_id), pRemNode->getIBNode()->name, hops);

  int res = pRemNode->processMad(inPortNum, item.madMsg);
  MSG_EXIT_FUNC;
  return(res);
}

int
IBMSDispatcher::routeMadToDest(
  madItem &item)
{
  MSG_ENTER_FUNC;

  MSGREG(inf1, 'V', "Routing mad to lid: $", "dispatcher");
  MSGSND(inf1, item.madMsg.addr.dlid);

  /*
     the over all routing algorithm is the same - go from current node to
     next node, but the method used to get the next node is based on the
     routing types.

     Since the traversal involves all node connectivity, port status and
     packet loss statistics - the dispatcher calls the nodes methods for
     obtaining the remote nodes:
     getRemoteNodeByOutPort(outPort, &pRemNode, &remPortNum)
     getRemoteNodeByLid(lid, &pRemNode, &remPortNum)

     These functions can return 1 if the routing was unsuccessful, including
     port state, fdb mismatch, packet drop, etc.

  */
  int res;
  if (item.madMsg.header.mgmt_class == 0x81)
    res = routeMadToDestByDR(item);
  else
    res = routeMadToDestByLid(item);

  MSG_EXIT_FUNC;
  return res;
}


