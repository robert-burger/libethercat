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

#ifndef LIBETHERCAT_MESSAGE_POOL_H
#define LIBETHERCAT_MESSAGE_POOL_H

#include <libosal/queue.h>

#include <libosal/types.h>
#include <libosal/mutex.h>
#include <libosal/semaphore.h>

#include "libethercat/common.h"

struct ec;

//! Message ID for asynchronous loop
typedef enum ec_async_message_id {
    EC_MSG_CHECK_GROUP,             //!< message type check group
    EC_MSG_CHECK_SLAVE              //!< message type check slave
} ec_async_message_id_t;
    
typedef osal_uint32_t ec_async_message_payload_t;

//! Message for asynchronous loop
typedef struct ec_message {
    ec_async_message_id_t id;       //!< index
    ec_async_message_payload_t payload;    
                                    //!< payload
} ec_message_t;

//! Message queue qentry
typedef struct ec_message_entry {
    TAILQ_ENTRY(ec_message_entry) qh;
                                    //!< handle to message entry queue
    
    ec_message_t msg;               //!< message itself
} ec_message_entry_t;

TAILQ_HEAD(ec_message_pool_queue, ec_message_entry);
typedef struct ec_message_pool_queue ec_message_pool_queue_t;

typedef struct ec_message_pool {
    ec_message_pool_queue_t queue;  //!< message pool queue
    osal_semaphore_t avail_cnt;     //!< available messages in pool queue
    osal_mutex_t lock;              //!< pool lock
} ec_message_pool_t;

#define EC_ASYNC_MESSAGE_LOOP_COUNT 100
typedef struct ec_async_loop {
    ec_message_entry_t entries[EC_ASYNC_MESSAGE_LOOP_COUNT];

    ec_message_pool_t avail;        //!< empty messages
    ec_message_pool_t exec;         //!< execute messages

    int loop_running;               //!< loop thread run flag
    osal_task_t loop_tid;           //!< loop thread id
    struct ec *pec;                 //!< ethercat master pointer

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
int ec_async_loop_create(ec_async_loop_t *paml, 
        struct ec *pec);

//! Destroys async message loop.
/*!
 * \param[in] paml  handle to async message loop
 *
 * \retval 0            On success
 * \retval error_code   On error
 */
int ec_async_loop_destroy(ec_async_loop_t *paml);

//! Execute asynchronous check group.
/*!
 * \param[in] paml  Handle to async message loop.
 * \param[in] gid   EtherCAT process data group id to check.
 */
void ec_async_check_group(ec_async_loop_t *paml, osal_uint16_t gid);

#ifdef __cplusplus
}
#endif

#endif // LIBETHERCAT_ASYNC_MESSAGE_LOOP_H

