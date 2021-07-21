/**
 * \file pool.c
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief ethercat datagram pool
 *
 * These are EtherCAT datagram pool specific configuration functions.
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

#include "libethercat/pool.h"
#include "libethercat/common.h"

#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

//! \brief Create a new data pool.
/*!
 * \param[out]  pp          Return pointer to newly created pool.
 * \param[in]   cnt         Number of entries in pool.
 * \param[in]   data_size   Size of data stored in each entry.
 *
 * \return 0 or negative error code
 */
int pool_open(pool_t **pp, size_t cnt, size_t data_size) {
    (*pp) = (pool_t *)malloc(sizeof(pool_t));
    if (!(*pp)) {
        return -ENOMEM;
    }

    pthread_mutex_init(&(*pp)->_pool_lock, NULL);
    pthread_mutex_lock(&(*pp)->_pool_lock);

    memset(&(*pp)->avail_cnt, 0, sizeof(sem_t));
    sem_init(&(*pp)->avail_cnt, 0, cnt);
    TAILQ_INIT(&(*pp)->avail);

    int i;
    for (i = 0; i < cnt; ++i) {
        pool_entry_t *entry = (pool_entry_t *)malloc(sizeof(pool_entry_t));
        memset(entry, 0, sizeof(pool_entry_t));

        entry->data_size = data_size;
        entry->data = (void *)malloc(data_size);
        memset(entry->data, 0, data_size);
        TAILQ_INSERT_TAIL(&(*pp)->avail, entry, qh);
    }
    
    pthread_mutex_unlock(&(*pp)->_pool_lock);

    return 0;
}

//! \brief Destroys a datagram pool.
/*!
 * \param[in]   pp          Pointer to pool.
 *
 * \return 0 or negative error code
 */
int pool_close(pool_t *pp) {
    if (!pp) {
        return -EINVAL;
    }
    
    pthread_mutex_lock(&pp->_pool_lock);

    pool_entry_t *entry;
    while ((entry = TAILQ_FIRST(&pp->avail)) != NULL) {
        TAILQ_REMOVE(&pp->avail, entry, qh);
        if (entry->data) { free(entry->data); }
        free(entry);
    }
    
    pthread_mutex_unlock(&pp->_pool_lock);
    pthread_mutex_destroy(&pp->_pool_lock);
    
    sem_destroy(&pp->avail_cnt);

    free(pp);
    
    return 0;
}

//! \brief Get a datagram from pool.
/*!
 * \param[in]   pp          Pointer to pool.
 * \param[out]  entry       Returns pointer to pool entry.
 * \param[in]   timeout     Timeout waiting for free entry.
 *
 * \return 0 or negative error code
 */
int pool_get(pool_t *pp, pool_entry_t **entry, ec_timer_t *timeout) {
    int ret = ENODATA;
    if (!pp || !entry) {
        return (ret = EINVAL);
    }

    struct timespec ts;

    if (timeout) {
        ts.tv_sec = timeout->sec;
        ts.tv_nsec = timeout->nsec;
    } else {
        ec_timer_t tim;
        ec_timer_gettime(&tim);
        ts.tv_sec = tim.sec;
        ts.tv_nsec = tim.nsec;
    }

    ret = sem_timedwait(&pp->avail_cnt, &ts);
    if (ret != 0) {
        if (errno != ETIMEDOUT) {
            perror("sem_timedwait");
        }

        *entry = NULL;
        return (ret = errno);
    }

    pthread_mutex_lock(&pp->_pool_lock);

    *entry = (pool_entry_t *)TAILQ_FIRST(&pp->avail);
    if (*entry) {
        TAILQ_REMOVE(&pp->avail, (pool_entry_t *)*entry, qh);
        ret = 0;
    }
    
    pthread_mutex_unlock(&pp->_pool_lock);

    return ret;
}

//! \brief Peek next entry from pool
/*!
 * \param[in]   pp          Pointer to pool.
 * \param[out]  entry       Returns pointer to pool entry. Be 
 *                          carefull, entry relies still in pool.
 *
 * \return 0 or negative error code
 */
int pool_peek(pool_t *pp, pool_entry_t **entry) {
    if (!pp || !entry) {
        return EINVAL;
    }
    
    pthread_mutex_lock(&pp->_pool_lock);
    *entry = (pool_entry_t *)TAILQ_FIRST(&pp->avail);
    pthread_mutex_unlock(&pp->_pool_lock);

    return ENODATA;
}
    
//! \brief Put entry back to pool.
/*!
 * \param[in]   pp          Pointer to pool.
 * \param[out]  entry       Entry to put back in pool.
 *
 * \return 0 or negative error code
 */
int pool_put(pool_t *pp, pool_entry_t *entry) {
    if (!pp || !entry) {
        return -EINVAL;
    }
    
    pthread_mutex_lock(&pp->_pool_lock);

    p_entry->user_cb = NULL;
    p_entry->user_arg = NULL;

    TAILQ_INSERT_TAIL(&pp->avail, (pool_entry_t *)entry, qh);
    sem_post(&pp->avail_cnt);
    
    pthread_mutex_unlock(&pp->_pool_lock);
    
    return 0;
}

