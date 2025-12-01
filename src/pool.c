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
 * libethercat is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * libethercat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libethercat (LICENSE.LGPL-V3); if not, write
 * to the Free Software Foundation, Inc., 51 Franklin Street, Fifth
 * Floor, Boston, MA  02110-1301, USA.
 *
 * Please note that the use of the EtherCAT technology, the EtherCAT
 * brand name and the EtherCAT logo is only permitted if the property
 * rights of Beckhoff Automation GmbH are observed. For further
 * information please contact Beckhoff Automation GmbH & Co. KG,
 * Hülshorstweg 20, D-33415 Verl, Germany (www.beckhoff.com) or the
 * EtherCAT Technology Group, Ostendstraße 196, D-90482 Nuremberg,
 * Germany (ETG, www.ethercat.org).
 *
 */
#ifdef HAVE_CONFIG_H
#include <libethercat/config.h>
#endif

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "libethercat/common.h"
#include "libethercat/ec.h"
#include "libethercat/error_codes.h"
#include "libethercat/pool.h"
// cppcheck-suppress misra-c2012-21.6
#include <stdio.h>

//! \brief Create a new data pool.
/*!
 * \param[out]  pp          Return pointer to newly created pool.
 * \param[in]   cnt         Number of entries in pool.
 * \param[in]   data_size   Size of data stored in each entry.
 *
 * \return EC_OK or error code
 */
int pool_open(pool_t* pp, osal_size_t cnt, pool_entry_t* entries) {
    assert(pp != NULL);
    int ret = EC_OK;

    osal_mutex_attr_t pool_lock_attr = OSAL_MUTEX_ATTR__PROTOCOL__INHERIT;
    osal_mutex_init(&pp->_pool_lock, &pool_lock_attr);
    osal_mutex_lock(&pp->_pool_lock);

    osal_semaphore_init(&pp->avail_cnt, 0, cnt);
    TAILQ_INIT(&pp->avail);

    osal_size_t i;
    for (i = 0; i < cnt; ++i) {
        // cppcheck-suppress misra-c2012-21.3
        pool_entry_t* entry = &entries[i];
        TAILQ_INSERT_TAIL(&pp->avail, entry, qh);
    }

    osal_mutex_unlock(&pp->_pool_lock);

    return ret;
}

//! \brief Destroys a datagram pool.
/*!
 * \param[in]   pp          Pointer to pool.
 *
 * \return EC_OK or error code
 */
int pool_close(pool_t* pp) {
    assert(pp != NULL);

    osal_mutex_lock(&pp->_pool_lock);

    pool_entry_t* entry = TAILQ_FIRST(&pp->avail);
    while (entry != NULL) {
        TAILQ_REMOVE(&pp->avail, entry, qh);
        entry = TAILQ_FIRST(&pp->avail);
    }

    osal_mutex_unlock(&pp->_pool_lock);
    osal_mutex_destroy(&pp->_pool_lock);

    osal_semaphore_destroy(&pp->avail_cnt);

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
int pool_get(pool_t* pp, pool_entry_t** entry, osal_timer_t* timeout) {
    assert(pp != NULL);
    assert(entry != NULL);

    int ret = EC_OK;
    *entry = NULL;

    if (timeout != NULL) {
        osal_retval_t local_ret = osal_semaphore_timedwait(&pp->avail_cnt, timeout);
        if (local_ret != OSAL_OK) {
            if (local_ret == OSAL_ERR_TIMEOUT) {
                ret = EC_ERROR_TIMEOUT;
            } else {
                ret = EC_ERROR_UNAVAILABLE;
            }
        }
    }

    if (ret == EC_OK) {
        osal_mutex_lock(&pp->_pool_lock);

        *entry = (pool_entry_t*)TAILQ_FIRST(&pp->avail);
        if ((*entry) != NULL) {
            TAILQ_REMOVE(&pp->avail, (pool_entry_t*)*entry, qh);
        } else {
            ret = EC_ERROR_UNAVAILABLE;
        }

        osal_mutex_unlock(&pp->_pool_lock);
    }

    return ret;
}

//! \brief Remove entry from pool
/*!
 * \param[in]   pp      Pointer to pool.
 * \param[in]   entry   Pool Entry to remove, got previously by pool_peek
 */
void pool_remove(pool_t* pp, pool_entry_t* entry) {
    assert(pp != NULL);
    assert(entry != NULL);

    osal_mutex_lock(&pp->_pool_lock);
    TAILQ_REMOVE(&pp->avail, entry, qh);
    osal_mutex_unlock(&pp->_pool_lock);
}

//! \brief Peek next entry from pool
/*!
 * \param[in]   pp          Pointer to pool.
 * \param[out]  entry       Returns pointer to pool entry. Be
 *                          carefull, entry relies still in pool.
 *
 * \return EC_OK or error code
 */
int pool_peek(pool_t* pp, pool_entry_t** entry) {
    assert(pp != NULL);
    assert(entry != NULL);
    int ret = EC_OK;

    osal_mutex_lock(&pp->_pool_lock);
    *entry = (pool_entry_t*)TAILQ_FIRST(&pp->avail);
    osal_mutex_unlock(&pp->_pool_lock);

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
void pool_put(pool_t* pp, pool_entry_t* entry) {
    assert(pp != NULL);
    assert(entry != NULL);

    osal_mutex_lock(&pp->_pool_lock);

    TAILQ_INSERT_TAIL(&pp->avail, (pool_entry_t*)entry, qh);
    osal_semaphore_post(&pp->avail_cnt);

    osal_mutex_unlock(&pp->_pool_lock);
}

//! \brief Put entry back to pool in front.
/*!
 * \param[in]   pp          Pointer to pool.
 * \param[out]  entry       Entry to put back in pool.
 */
void pool_put_head(pool_t* pp, pool_entry_t* entry) {
    assert(pp != NULL);
    assert(entry != NULL);

    osal_mutex_lock(&pp->_pool_lock);

    TAILQ_INSERT_HEAD(&pp->avail, (pool_entry_t*)entry, qh);
    osal_semaphore_post(&pp->avail_cnt);

    osal_mutex_unlock(&pp->_pool_lock);
}
