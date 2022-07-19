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

//! get next free index entry
/*!
 * \param idx_q pointer to index queue
 * \param entry return entry of next free index 
 * \return 0 on succes, otherwise error code
 */
int ec_index_get(idx_queue_t *idx_q, struct idx_entry **entry) {
    int ret = -1;

    assert(idx_q != NULL);
    assert(entry != NULL);

    pthread_mutex_lock(&idx_q->lock);

    *entry = (idx_entry_t *)TAILQ_FIRST(&idx_q->q);
    if ((*entry) != NULL) {
        TAILQ_REMOVE(&idx_q->q, *entry, qh);
        ret = 0;
    
        while (sem_trywait(&(*entry)->waiter) == 0) {
            ;
        }
    }

    
    pthread_mutex_unlock(&idx_q->lock);

    return ret;
}

//! returns index entry
/*!
 * \param idx_q pointer to index queue
 * \param entry return index entry 
 * \return 0 on succes, otherwise error code
 */
int ec_index_put(idx_queue_t *idx_q, struct idx_entry *entry) {
    assert(idx_q != NULL);
    assert(entry != NULL);

    pthread_mutex_lock(&idx_q->lock);
    TAILQ_INSERT_TAIL(&idx_q->q, entry, qh);
    pthread_mutex_unlock(&idx_q->lock);

    return 0;
}

//! initialize index queue structure
/*!
 * initialize index queue structure and fill in 256 indicex for ethercat frames
 *
 * \param idx_q pointer to index queue structure
 * \return 0 on success
 */
int ec_index_init(idx_queue_t *idx_q, size_t max_index) {
    uint32_t i;

    assert(idx_q != NULL);

    pthread_mutex_init(&idx_q->lock, NULL);
    
    // fill index queue
    TAILQ_INIT(&idx_q->q);
    for (i = 0; i < max_index; ++i) {
        // cppcheck-suppress misra-c2012-21.3
        idx_entry_t *entry = (idx_entry_t *)malloc(sizeof(idx_entry_t));
        entry->idx = i;
        (void)memset(&entry->waiter, 0, sizeof(sem_t));
        sem_init(&entry->waiter, 0, 0);
        (void)ec_index_put(idx_q, entry);
    }

    return 0;
}

//! deinitialize index queue structure
/*!
 * deinitialize index queue structure and
 * clear all indicex for ethercat frames
 *
 * \param idx_q pointer to index queue structure
 */
void ec_index_deinit(idx_queue_t *idx_q) {
    assert(idx_q != NULL);

    idx_entry_t *idx = TAILQ_FIRST(&idx_q->q);
    while (idx != NULL) {
        TAILQ_REMOVE(&idx_q->q, idx, qh);
        sem_destroy(&idx->waiter);

        // cppcheck-suppress misra-c2012-21.3
        free(idx);
        idx = TAILQ_FIRST(&idx_q->q);
    }

    pthread_mutex_destroy(&idx_q->lock);
}

