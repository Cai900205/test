/*
 * Copyright (C) Mellanox Technologies Ltd. 2001-2013.  ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of Mellanox Technologies Ltd.
 * (the "Company") and all right, title, and interest in and to the software product,
 * including all associated intellectual property rights, are and shall
 * remain exclusively with the Company.
 *
 * This software is made available under either the GPL v2 license or a commercial license.
 * If you wish to obtain a commercial license, please contact Mellanox at support@mellanox.com.
 */



#ifndef NEIGHBOUR_OBSERVER_H
#define NEIGHBOUR_OBSERVER_H

#include "vma/util/sys_vars.h"
#include "vma/infra/subject_observer.h"

class neigh_observer : public observer
{
public:
	virtual transport_type_t get_obs_transport_type() const = 0;
};

#endif /* NEIGHBOUR_OBSERVER_H */
