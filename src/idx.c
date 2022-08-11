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

#include <string.h>
#include <assert.h>

#include "libethercat/idx.h"
#include "libethercat/ec.h"
#include "libethercat/memory.h"
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

    osal_spinlock_lock(&idx_q->lock);

    *entry = (idx_entry_t *)TAILQ_FIRST(&idx_q->q);
    if ((*entry) != NULL) {
        TAILQ_REMOVE(&idx_q->q, *entry, qh);
        ret = EC_OK;
    
        osal_binary_semaphore_trywait(&(*entry)->waiter);
    }

    
    osal_spinlock_unlock(&idx_q->lock);

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

    osal_spinlock_lock(&idx_q->lock);
    TAILQ_INSERT_TAIL(&idx_q->q, entry, qh);
    osal_spinlock_unlock(&idx_q->lock);
}

//! Initialize index queue structure.
/*!
 * Initialize index queue structure and fill in 256 indicex for ethercat frames.
 *
 * \param[in]   idx_q   Pointer to index queue structure.
 *
 * \return EC_OK 0 on success, oherwise error code
 */
int ec_index_init(idx_queue_t *idx_q, osal_size_t max_index) {
    osal_uint32_t i;
    int ret = EC_OK;

    assert(idx_q != NULL);

    osal_spinlock_init(&idx_q->lock, NULL);
    
    // fill index queue
    TAILQ_INIT(&idx_q->q);
    for (i = 0; i < max_index; ++i) {
        // cppcheck-suppress misra-c2012-21.3
        idx_entry_t *entry = (idx_entry_t *)ec_malloc(sizeof(idx_entry_t));
        if (entry == NULL) {
            ret = EC_ERROR_OUT_OF_MEMORY;
            break;
        }

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

        // cppcheck-suppress misra-c2012-21.3
        ec_free(idx);
        idx = TAILQ_FIRST(&idx_q->q);
    }

    osal_spinlock_destroy(&idx_q->lock);
}

