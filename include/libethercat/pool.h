/**
 * \file pool.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief Data pool implementation
 *
 * These are EtherCAT pool specific configuration functions.
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

#ifndef POOL_H
#define POOL_H

#include <libosal/queue.h>

#include <libosal/types.h>
#include <libosal/mutex.h>
#include <libosal/semaphore.h>

#include "libethercat/common.h"

#define LEC_MAX_POOL_DATA_SIZE  1600

//! \brief Pool queue entry. 
typedef struct pool_entry {
    void (*user_cb)(void *user_arg, struct pool_entry *p);  //!< \brief User callback.
    void *user_arg;                                         //!< \brief User argument for user_cb.

    TAILQ_ENTRY(pool_entry) qh;                             //!< \brief Queue handle of pool objects.
    
    osal_uint8_t data[LEC_MAX_POOL_DATA_SIZE];              //!< \brief Data entry.
} pool_entry_t;

//! queue head for pool queue
TAILQ_HEAD(pool_queue, pool_entry);

//! the datagram pool itself
typedef struct pool {    
    struct pool_queue avail;                                //!< \brief Queue with available datagrams.
    osal_semaphore_t avail_cnt;                             //!< \brief Available datagrams in pool.
    osal_mutex_t _pool_lock;                                //!< \brief Pool lock.
} pool_t;

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Create a new data pool.
/*!
 * \param[out]  pp          Return pointer to newly created pool.
 * \param[in]   cnt         Number of entries in pool.
 * \param[in]   data_size   Size of data stored in each entry.
 *
 * \return EC_OK or error code
 */
int pool_open(pool_t *pp, osal_size_t cnt, pool_entry_t *entries);

//! \brief Destroys a datagram pool.
/*!
 * \param[in]   pp          Pointer to pool.
 *
 * \return EC_OK or error code
 */
int pool_close(pool_t *pp);

//! \brief Get a datagram from pool.
/*!
 * \param[in]   pp          Pointer to pool.
 * \param[out]  entry       Returns pointer to pool entry.
 * \param[in]   timeout     Timeout waiting for free entry.
 *
 * \return EC_OK or error code
 */
int pool_get(pool_t *pp, pool_entry_t **entry, osal_timer_t *timeout);

//! \brief Peek next entry from pool
/*!
 * \param[in]   pp          Pointer to pool.
 * \param[out]  entry       Returns pointer to pool entry. Be 
 *                          carefull, entry relies still in pool.
 *
 * \return EC_OK or error code
 */
int pool_peek(pool_t *pp, pool_entry_t **entry);
    
//! \brief Put entry back to pool.
/*!
 * \param[in]   pp          Pointer to pool.
 * \param[out]  entry       Entry to put back in pool.
 */
void pool_put(pool_t *pp, pool_entry_t *entry);

//! \brief Put entry back to pool in front.
/*!
 * \param[in]   pp          Pointer to pool.
 * \param[out]  entry       Entry to put back in pool.
 */
void pool_put_head(pool_t *pp, pool_entry_t *entry);

#ifdef __cplusplus
}
#endif

#endif // POOL_H

