//! ethercat index queueu
/*!
 * \author Robert Burger
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

#include <string.h>

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

    pthread_mutex_lock(&idx_q->lock);

    *entry = (idx_entry_t *)TAILQ_FIRST(&idx_q->q);
    if (*entry) {
        TAILQ_REMOVE(&idx_q->q, *entry, qh);
        ret = 0;
    
        while (sem_trywait(&(*entry)->waiter) == 0)
            ;
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
    if (!idx_q || !entry)
        return -1;

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
int ec_index_init(idx_queue_t *idx_q) {
    int i;
    pthread_mutex_init(&idx_q->lock, NULL);
    
    // fill index queue
    TAILQ_INIT(&idx_q->q);
    for (i = 0; i < 255; ++i) {
        idx_entry_t *entry = (idx_entry_t *)malloc(sizeof(idx_entry_t));
        entry->idx = i;
        memset(&entry->waiter, 0, sizeof(sem_t));
        sem_init(&entry->waiter, 0, 0);
        ec_index_put(idx_q, entry);
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
    idx_entry_t *idx;
    while ((idx = TAILQ_FIRST(&idx_q->q)) != NULL) {
        TAILQ_REMOVE(&idx_q->q, idx, qh);
        free(idx);
    }

    pthread_mutex_destroy(&idx_q->lock);
}

