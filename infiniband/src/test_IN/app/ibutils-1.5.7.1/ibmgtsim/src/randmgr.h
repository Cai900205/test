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

/****h* IBMS/RandomManager
* NAME
*	A singleton for random numbers generation.
*
* DESCRIPTION
*	The random number generator singleton is required to
*  enable test reproduction by serializing all random number
*  generation.
*
* AUTHOR
*	Eitan Zahavi, Mellanox
*
*********/

#ifndef IBMS_RANDMGR_H
#define IBMS_RANDMGR_H

#include <pthread.h>

class RandomMgr {
  /* the random generator seed */
  unsigned int randomSeed;

  /* lock - enables locking the simulator */
  pthread_mutex_t lock;

 public:
  /* constructor */
  RandomMgr() {
    randomSeed = 0;
    pthread_mutex_init( &lock, NULL );
  };

  /* set the random number seed */
  int setRandomSeed( int seed );

  /* get a random floating point number 0.0 - 1.0 */
  float random();
};

/* get the singleton pointer */
RandomMgr *RandMgr();

#endif /* IBMS_RANDMGR_H */
