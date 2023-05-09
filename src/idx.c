/**
 * \file idx.c
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

#include <libethercat/config.h>

#include <string.h>
#include <assert.h>

#include "libethercat/idx.h"
#include "libethercat/ec.h"
#include "libethercat/error_codes.h"

//! Get next free index entry.
/*!
 * \param[in]   idx_q   Pointer to index queue.
 * \param[out]  entry   Return entry of next free index.
 *
 * \return EC_OK on succes, otherwise error code
 */
int ec_index_get(idx_queue_t *idx_q, struct idx_entry **entry) {
    int ret = EC_ERROR_OUT_OF_INDICES;

    assert(idx_q != NULL);
    assert(entry != NULL);

    osal_mutex_lock(&idx_q->lock);

    *entry = (idx_entry_t *)TAILQ_FIRST(&idx_q->q);
    if ((*entry) != NULL) {
        TAILQ_REMOVE(&idx_q->q, *entry, qh);
        ret = EC_OK;
    
        osal_binary_semaphore_trywait(&(*entry)->waiter);
    }

    osal_mutex_unlock(&idx_q->lock);

    return ret;
}

//! Returns index entry
/*!
 * \param[in]  idx_q    Pointer to index queue.
 * \param[in]  entry    Return index entry.
 */
void ec_index_put(idx_queue_t *idx_q, struct idx_entry *entry) {
    assert(idx_q != NULL);
    assert(entry != NULL);

    osal_mutex_lock(&idx_q->lock);
    TAILQ_INSERT_TAIL(&idx_q->q, entry, qh);
    osal_mutex_unlock(&idx_q->lock);
}

//! Initialize index queue structure.
/*!
 * Initialize index queue structure and fill in 256 indicex for ethercat frames.
 *
 * \param[in]   idx_q   Pointer to index queue structure.
 *
 * \return EC_OK 0 on success, oherwise error code
 */
int ec_index_init(idx_queue_t *idx_q) {
    osal_uint32_t i;
    int ret = EC_OK;

    assert(idx_q != NULL);

    osal_mutex_init(&idx_q->lock, NULL);
    
    // fill index queue
    TAILQ_INIT(&idx_q->q);
    for (i = 0; i < LEC_MAX_INDEX; ++i) {
        idx_entry_t *entry = &idx_q->entries[i];
        entry->idx = i;
        osal_binary_semaphore_init(&entry->waiter, NULL);
        (void)ec_index_put(idx_q, entry);
    }

    if (ret != EC_OK) {
        ec_index_deinit(idx_q);
    }

    return ret;
}

//! Deinitialize index queue structure.
/*!
 * Deinitialize index queue structure and clear all indicex for ethercat frames.
 *
 * \param[in]   idx_q   Pointer to index queue structure.
 */
void ec_index_deinit(idx_queue_t *idx_q) {
    assert(idx_q != NULL);

    idx_entry_t *idx = TAILQ_FIRST(&idx_q->q);
    while (idx != NULL) {
        TAILQ_REMOVE(&idx_q->q, idx, qh);
        osal_binary_semaphore_destroy(&idx->waiter);

        idx = TAILQ_FIRST(&idx_q->q);
    }

    osal_mutex_destroy(&idx_q->lock);
}

