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

#ifndef LIBETHERCAT_IDX_H
#define LIBETHERCAT_IDX_H

#include <libosal/binary_semaphore.h>
#include <libosal/mutex.h>
#include <libosal/queue.h>
#include <libosal/types.h>

#include "libethercat/common.h"

#define LEC_MAX_INDEX 256

//! index entry
typedef struct idx_entry {
    osal_uint8_t idx;                //!< \brief Datagram index.
    osal_binary_semaphore_t waiter;  //!< \brief Waiter semaphore for synchronous access.
    TAILQ_ENTRY(idx_entry) qh;       //!< \brief Queue handle
} idx_entry_t;
TAILQ_HEAD(idx_entry_queue, idx_entry);

//! index queue
typedef struct idx_queue {
    osal_mutex_t lock;                   //!< \brief Queue lock, prevent concurrent queue access.
    idx_entry_t entries[LEC_MAX_INDEX];  //!< \brief Static queue entries, do not use directly.
    struct idx_entry_queue q;            //!< \brief The head of the index queue.
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
int ec_index_init(idx_queue_t* idx_q);

//! Deinitialize index queue structure.
/*!
 * Deinitialize index queue structure and clear all indicex for ethercat frames.
 *
 * \param[in]   idx_q   Pointer to index queue structure.
 */
void ec_index_deinit(idx_queue_t* idx_q);

//! Get next free index entry.
/*!
 * \param[in]   idx_q   Pointer to index queue.
 * \param[out]  entry   Return entry of next free index.
 *
 * \return EC_OK on succes, otherwise error code
 */
int ec_index_get(idx_queue_t* idx_q, struct idx_entry** entry);

//! Returns index entry
/*!
 * \param[in]  idx_q    Pointer to index queue.
 * \param[in]  entry    Return index entry.
 */
void ec_index_put(idx_queue_t* idx_q, struct idx_entry* entry);

#ifdef __cplusplus
}
#endif

#endif  // LIBETHERCAT_IDX_H
