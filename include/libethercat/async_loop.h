/**
 * \file message_pool.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 11 Nov 2016
 *
 * \brief EtherCAT asynchronous message loop
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

#ifndef LIBETHERCAT_MESSAGE_POOL_H
#define LIBETHERCAT_MESSAGE_POOL_H

#include <libosal/mutex.h>
#include <libosal/queue.h>
#include <libosal/semaphore.h>
#include <libosal/task.h>
#include <libosal/types.h>

#include "libethercat/common.h"

struct ec;

//! Message ID for asynchronous loop
typedef enum ec_async_message_id {
    EC_MSG_CHECK_GROUP,  //!< \brief message type check group
    EC_MSG_CHECK_SLAVE,  //!< \brief message type check slave
    EC_MSG_CHECK_ALL     //!< \brief message type check all slaves
} ec_async_message_id_t;

typedef osal_uint32_t ec_async_message_payload_t;

//! Message for asynchronous loop
typedef struct ec_message {
    ec_async_message_id_t id;  //!< \brief index
    ec_async_message_payload_t payload;
    //!< \brief payload
} ec_message_t;

//! Message queue qentry
typedef struct ec_message_entry {
    TAILQ_ENTRY(ec_message_entry) qh;
    //!< \brief handle to message entry queue

    ec_message_t msg;  //!< \brief message itself
} ec_message_entry_t;

TAILQ_HEAD(ec_message_pool_queue, ec_message_entry);
typedef struct ec_message_pool_queue ec_message_pool_queue_t;

typedef struct ec_message_pool {
    ec_message_pool_queue_t queue;  //!< \brief message pool queue
    osal_semaphore_t avail_cnt;     //!< \brief available messages in pool queue
    osal_mutex_t lock;              //!< \brief pool lock
} ec_message_pool_t;

#define EC_ASYNC_MESSAGE_LOOP_COUNT 100
typedef struct ec_async_loop {
    ec_message_entry_t entries[EC_ASYNC_MESSAGE_LOOP_COUNT];

    ec_message_pool_t avail;  //!< \brief empty messages
    ec_message_pool_t exec;   //!< \brief execute messages

    int loop_running;      //!< \brief loop thread run flag
    osal_task_t loop_tid;  //!< \brief loop thread id
    struct ec* pec;        //!< \brief ethercat master pointer

    osal_timer_t next_check_group;
} ec_async_loop_t;

#ifdef __cplusplus
extern "C" {
#endif

//! creates a new async message loop
/*!
 * \param[out] paml         Return newly created handle to async message loop.
 * \param[in] pec           Pointer to ethercat master structure,
 *                          which you got from \link ec_open \endlink.
 * \retval 0            On success
 * \retval error_code   On error
 */
int ec_async_loop_create(ec_async_loop_t* paml, struct ec* pec);

//! Destroys async message loop.
/*!
 * \param[in] paml  handle to async message loop
 *
 * \retval 0            On success
 * \retval error_code   On error
 */
int ec_async_loop_destroy(ec_async_loop_t* paml);

//! Execute asynchronous check group.
/*!
 * \param[in] paml  Handle to async message loop.
 * \param[in] gid   EtherCAT process data group id to check.
 */
void ec_async_check_group(ec_async_loop_t* paml, osal_uint16_t gid);

//! Execute asynchronous check all slaves.
/*!
 * \param[in] paml  Handle to async message loop.
 */
void ec_async_check_all(ec_async_loop_t* paml);

// Execute one async check step.
/*!
 * This function is usually called by Async loop thread.
 * \param[in] paml  Handle to async message loop.
 */
void ec_async_loop_step(ec_async_loop_t* paml, osal_timer_t* to);

#ifdef __cplusplus
}
#endif

#endif  // LIBETHERCAT_ASYNC_MESSAGE_LOOP_H
