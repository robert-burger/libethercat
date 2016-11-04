//! ethercat message pool
/*!
 * author: Robert Burger
 *
 * $Id$
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

#ifndef __LIBETHERCAT_ASYNC_MESSAGE_LOOP_H__
#define __LIBETHERCAT_ASYNC_MESSAGE_LOOP_H__

#include <sys/queue.h>
#include <stdlib.h>
#include <stdint.h>
#include <semaphore.h>

#include "libethercat/timer.h"

struct ec;

typedef enum {
    EC_MSG_CHECK_GROUP,
    EC_MSG_CHECK_SLAVE
} ec_async_message_id_t;
    
typedef union ec_async_message_payload {
    void *ptr;
    uint32_t group_id;
    uint32_t slave_id;
} ec_async_message_payload_t;

typedef struct ec_message {
    ec_async_message_id_t id;
    ec_async_message_payload_t payload;    
} ec_message_t;

typedef struct __attribute__((__packed__)) ec_message_entry {
    TAILQ_ENTRY(ec_message_entry) qh;
    
    ec_message_t msg;
} ec_message_entry_t;

TAILQ_HEAD(ec_message_pool_queue, ec_message_entry);
typedef struct ec_message_pool_queue ec_message_pool_queue_t;

typedef struct ec_message_pool {
    ec_message_pool_queue_t queue;  //! message pool queue
    sem_t avail_cnt;                //! available messages in pool queue
    pthread_mutex_t lock;           //! pool lock
} ec_message_pool_t;

typedef struct ec_async_message_loop {
    ec_message_pool_t avail;        //! empty messages
    ec_message_pool_t exec;         //! execute messages

    int loop_running;               //! loop thread run flag
    pthread_t loop_tid;             //! loop thread id
    struct ec *pec;                 //! ethercat master pointer

    ec_timer_t next_check_group;
} ec_async_message_loop_t;

#ifdef __cplusplus
extern "C" {
#endif

#define EC_ASYNC_MESSAGE_LOOP_COUNT 100

//! creates a new async message loop
/*!
 * \param ppaml return newly created handle to async message loop
 * \param pec pointer to ethercat master
 * \return 0 or error code
 */
int ec_async_message_loop_create(ec_async_message_loop_t **ppaml, struct ec *pec);

//! destroys async message loop
/*!
 * \param paml handle to async message loop
 * \return 0 or error code
 */
int ec_async_message_pool_destroy(ec_async_message_loop_t *paml);

//! execute asynchronous check group
/*!
 * \param paml handle to async message loop
 * \param gid group id to check
 */
void ec_async_check_group(ec_async_message_loop_t *paml, uint16_t gid);

#ifdef __cplusplus
}
#endif

#endif // __LIBETHERCAT_ASYNC_MESSAGE_LOOP_H__

