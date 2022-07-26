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
 * If not, see <www.gnu.org/licenses/>.
 */

#include "config.h"

#include "libethercat/pool.h"
#include "libethercat/common.h"
#include "libethercat/ec.h"
#include "libethercat/error_codes.h"

#include <assert.h>
#include <errno.h>
// cppcheck-suppress misra-c2012-21.10
#include <time.h>
#include <string.h>
// cppcheck-suppress misra-c2012-21.6
#include <stdio.h>
#include <pthread.h>

//! \brief Create a new data pool.
/*!
 * \param[out]  pp          Return pointer to newly created pool.
 * \param[in]   cnt         Number of entries in pool.
 * \param[in]   data_size   Size of data stored in each entry.
 *
 * \return EC_OK or error code
 */
int pool_open(pool_t **pp, size_t cnt, size_t data_size) {
    assert(pp != NULL);
    int ret = EC_OK;

    // cppcheck-suppress misra-c2012-21.3
    (*pp) = (pool_t *)malloc(sizeof(pool_t));
    if (!(*pp)) {
        ret = EC_ERROR_OUT_OF_MEMORY;
    } else {
        pthread_mutex_init(&(*pp)->_pool_lock, NULL);
        pthread_mutex_lock(&(*pp)->_pool_lock);

        (void)memset(&(*pp)->avail_cnt, 0, sizeof(sem_t));
        sem_init(&(*pp)->avail_cnt, 0, cnt);
        TAILQ_INIT(&(*pp)->avail);

        size_t i;
        for (i = 0; i < cnt; ++i) {
            // cppcheck-suppress misra-c2012-21.3
            pool_entry_t *entry = (pool_entry_t *)malloc(sizeof(pool_entry_t));
            (void)memset(entry, 0, sizeof(pool_entry_t));

            entry->data_size = data_size;
            // cppcheck-suppress misra-c2012-21.3
            entry->data = (void *)malloc(data_size);
            (void)memset(entry->data, 0, data_size);
            TAILQ_INSERT_TAIL(&(*pp)->avail, entry, qh);
        }

        pthread_mutex_unlock(&(*pp)->_pool_lock);
    }

    return ret;
}

//! \brief Destroys a datagram pool.
/*!
 * \param[in]   pp          Pointer to pool.
 *
 * \return EC_OK or error code
 */
int pool_close(pool_t *pp) {
    assert(pp != NULL);
    
    pthread_mutex_lock(&pp->_pool_lock);

    pool_entry_t *entry = TAILQ_FIRST(&pp->avail);
    while (entry != NULL) {
        TAILQ_REMOVE(&pp->avail, entry, qh);
        if (entry->data != NULL) { 
            // cppcheck-suppress misra-c2012-21.3
            free(entry->data); 
            entry->data = NULL; 
        }

        // cppcheck-suppress misra-c2012-21.3
        free(entry);

        entry = TAILQ_FIRST(&pp->avail);
    }
    
    pthread_mutex_unlock(&pp->_pool_lock);
    pthread_mutex_destroy(&pp->_pool_lock);
    
    sem_destroy(&pp->avail_cnt);

    // cppcheck-suppress misra-c2012-21.3
    free(pp);
    
    return EC_OK;
}

//! \brief Get a datagram from pool.
/*!
 * \param[in]   pp          Pointer to pool.
 * \param[out]  entry       Returns pointer to pool entry.
 * \param[in]   timeout     Timeout waiting for free entry.
 *
 * \return EC_OK or error code
 */
int pool_get(pool_t *pp, pool_entry_t **entry, ec_timer_t *timeout) {
    assert(pp != NULL);
    assert(entry != NULL);

    int ret = EC_OK;
    struct timespec ts;

    if (timeout != NULL) {
        ts.tv_sec = timeout->sec;
        ts.tv_nsec = timeout->nsec;
    } else {
        ec_timer_t tim;
        (void)ec_timer_gettime(&tim);
        ts.tv_sec = tim.sec;
        ts.tv_nsec = tim.nsec;
    }

    while (ret == EC_OK) {
        int local_ret = sem_timedwait(&pp->avail_cnt, &ts);
        int tmp_errno = errno;
        if (local_ret == 0) {
            break;
        }

        if (tmp_errno != ETIMEDOUT) {
            perror("sem_timedwait");
        } else {
            *entry = NULL;
            ret = EC_ERROR_TIMEOUT;
        }
    }

    if (ret == EC_OK) {
        pthread_mutex_lock(&pp->_pool_lock);

        *entry = (pool_entry_t *)TAILQ_FIRST(&pp->avail);
        if ((*entry) != NULL) {
            TAILQ_REMOVE(&pp->avail, (pool_entry_t *)*entry, qh);
        } else {
            ret = EC_ERROR_UNAVAILABLE;
        }

        pthread_mutex_unlock(&pp->_pool_lock);
    }

    return ret;
}

//! \brief Peek next entry from pool
/*!
 * \param[in]   pp          Pointer to pool.
 * \param[out]  entry       Returns pointer to pool entry. Be 
 *                          carefull, entry relies still in pool.
 *
 * \return EC_OK or error code
 */
int pool_peek(pool_t *pp, pool_entry_t **entry) {
    assert(pp != NULL);
    assert(entry != NULL);
    int ret = EC_OK;
    
    pthread_mutex_lock(&pp->_pool_lock);
    *entry = (pool_entry_t *)TAILQ_FIRST(&pp->avail);
    pthread_mutex_unlock(&pp->_pool_lock);

    if ((*entry) == NULL) {
        ret = EC_ERROR_UNAVAILABLE;
    }

    return ret;
}
    
//! \brief Put entry back to pool.
/*!
 * \param[in]   pp          Pointer to pool.
 * \param[out]  entry       Entry to put back in pool.
 */
void pool_put(pool_t *pp, pool_entry_t *entry) {
    assert(pp != NULL);
    assert(entry != NULL);
    
    pthread_mutex_lock(&pp->_pool_lock);

    TAILQ_INSERT_TAIL(&pp->avail, (pool_entry_t *)entry, qh);
    sem_post(&pp->avail_cnt);
    
    pthread_mutex_unlock(&pp->_pool_lock);
}

//! \brief Put entry back to pool in front.
/*!
 * \param[in]   pp          Pointer to pool.
 * \param[out]  entry       Entry to put back in pool.
 */
void pool_put_head(pool_t *pp, pool_entry_t *entry) {
    assert(pp != NULL);
    assert(entry != NULL);
    
    pthread_mutex_lock(&pp->_pool_lock);

    TAILQ_INSERT_HEAD(&pp->avail, (pool_entry_t *)entry, qh);
    sem_post(&pp->avail_cnt);
    
    pthread_mutex_unlock(&pp->_pool_lock);
}

