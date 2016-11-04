//! ethercat datagram pool
/*!
 * author: Robert Burger
 *
 * $Id$
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

#ifndef __LIBETHERCAT_DATAGRAM_POOL_H__
#define __LIBETHERCAT_DATAGRAM_POOL_H__

#include <sys/queue.h>
#include <stdlib.h>
#include <stdint.h>
#include <semaphore.h>

#include "libethercat/datagram.h"
#include "libethercat/timer.h"

typedef struct __attribute__((__packed__)) datagram_entry {
    void (*user_cb)(void *user_arg, struct datagram_entry *p);
    void *user_arg;
    TAILQ_ENTRY(datagram_entry) qh;
    
    ec_datagram_t datagram;
} datagram_entry_t;

TAILQ_HEAD(datagram_pool_queue, datagram_entry);

typedef struct datagram_pool {    
    struct datagram_pool_queue avail;
    sem_t avail_cnt;

    pthread_mutex_t _pool_lock;
} datagram_pool_t;

#ifdef __cplusplus
extern "C" {
#endif

//! open a new datagram pool
/*!
 * \param pp return datagram_pool 
 * \param cnt number of packets in pool
 * \return 0 or negative error code
 */
int datagram_pool_open(datagram_pool_t **pp, size_t cnt);

//! destroys a datagram pool
/*!
 * \param pp datagram_pool handle
 * \return 0 or negative error code
 */
int datagram_pool_close(datagram_pool_t *pp);

//! get a datagram from datagram_pool
/*!
 * \param pp datagram_pool handle
 * \param datagram ec datagram pointer
 * \param timeout timeout waiting for packet
 * \return 0 or negative error code
 */
int datagram_pool_get(datagram_pool_t *pp, 
        datagram_entry_t **datagram, ec_timer_t *timeout);

//! get next datagram length from datagram_pool
/*!
 * \param pp datagram_pool handle
 * \param len datagram length
 * \return 0 or negative error code
 */
int datagram_pool_get_next_len(datagram_pool_t *pp, size_t *len);
    
//! return a datagram to datagram_pool
/*!
 * \param pp datagram_pool handle
 * \param datagram ec datagram pointer
 * \return 0 or negative error code
 */
int datagram_pool_put(datagram_pool_t *pp, datagram_entry_t *datagram);

#ifdef __cplusplus
}
#endif

#endif // __LIBETHERCAT_DATAGRAM_POOL_H__

