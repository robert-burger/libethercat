/**
 * \file idx.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 11 Nov 2016
 *
 * \brief ethercat index
 *
 */

/*
 * This file is part of libethercat.
 *
 * libethercat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libethercat is distributed in the hope that 
 * it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libethercat
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LIBETHERCAT_IDX_H__
#define __LIBETHERCAT_IDX_H__

#include <sys/queue.h>
#include <semaphore.h>

#include "libethercat/common.h"

//! index entry
typedef struct idx_entry {
    uint8_t idx;                //!< datagram index
    sem_t waiter;               //!< waiter semaphore for synchronous access
    struct ec *pec;             //!< pointer to ethercat master structure

    TAILQ_ENTRY(idx_entry) qh;  //!< queue handle
} idx_entry_t;
TAILQ_HEAD(idx_entry_queue, idx_entry);


//! index queue
typedef struct idx_queue {
    pthread_mutex_t lock;       //!< queue lock
                                /*!<
                                 * prevent concurrent queue access
                                 */

    struct idx_entry_queue q;   //!< the head of the index queue
} idx_queue_t;

#ifdef __cplusplus
extern "C" {
#endif


//! initialize index queue structure
/*!
 * initialize index queue structure and fill in 256 indicex for ethercat frames
 *
 * \param idx_q pointer to index queue structure
 * \return 0 on success
 */
int ec_index_init(idx_queue_t *idx_q, size_t max_index);

//! deinitialize index queue structure
/*!
 * deinitialize index queue structure and
 * clear all indicex for ethercat frames
 *
 * \param idx_q pointer to index queue structure
 */
void ec_index_deinit(idx_queue_t *idx_q);

//! get next free index entry
/*!
 * \param idx_q pointer to index queue structure
 * \param entry return entry of next free index 
 * \return 0 on succes, otherwise error code
 */
int ec_index_get(idx_queue_t *idx_q, struct idx_entry **entry);

//! returns index entry
/*!
 * \param idx_q pointer to index queue structure
 * \param entry return index entry 
 * \return 0 on succes, otherwise error code
 */
int ec_index_put(idx_queue_t *idx_q, struct idx_entry *entry);


#ifdef __cplusplus
}
#endif

#endif // __LIBETHERCAT_IDX_H__

