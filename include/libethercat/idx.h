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

#ifndef LIBETHERCAT_IDX_H
#define LIBETHERCAT_IDX_H

#include <sys/queue.h>
#include <libosal/binary_semaphore.h>
#include <libosal/mutex.h>

#include "libethercat/common.h"

//! index entry
typedef struct idx_entry {
    uint8_t idx;                    //!< datagram index
    osal_binary_semaphore_t waiter; //!< waiter semaphore for synchronous access
    struct ec *pec;                 //!< pointer to ethercat master structure

    TAILQ_ENTRY(idx_entry) qh;      //!< queue handle
} idx_entry_t;
TAILQ_HEAD(idx_entry_queue, idx_entry);


//! index queue
typedef struct idx_queue {
    osal_mutex_t lock;          //!< queue lock
                                /*!<
                                 * prevent concurrent queue access
                                 */

    struct idx_entry_queue q;   //!< the head of the index queue
} idx_queue_t;

#ifdef __cplusplus
extern "C" {
#endif

//! Initialize index queue structure.
/*!
 * Initialize index queue structure and fill in 256 indicex for ethercat frames.
 *
 * \param[in]   idx_q   Pointer to index queue structure.
 *
 * \return EC_OK 0 on success, oherwise error code
 */
int ec_index_init(idx_queue_t *idx_q, size_t max_index);

//! Deinitialize index queue structure.
/*!
 * Deinitialize index queue structure and clear all indicex for ethercat frames.
 *
 * \param[in]   idx_q   Pointer to index queue structure.
 */
void ec_index_deinit(idx_queue_t *idx_q);

//! Get next free index entry.
/*!
 * \param[in]   idx_q   Pointer to index queue.
 * \param[out]  entry   Return entry of next free index.
 *
 * \return EC_OK on succes, otherwise error code
 */
int ec_index_get(idx_queue_t *idx_q, struct idx_entry **entry);

//! Returns index entry
/*!
 * \param[in]  idx_q    Pointer to index queue.
 * \param[in]  entry    Return index entry.
 */
void ec_index_put(idx_queue_t *idx_q, struct idx_entry *entry);


#ifdef __cplusplus
}
#endif

#endif // LIBETHERCAT_IDX_H

