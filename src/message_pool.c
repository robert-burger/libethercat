/**
 * \file message_pool.c
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 11 Nov 2016
 *
 * \brief ethercat async message loop
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

#include "libethercat/ec.h"
#include "libethercat/error_codes.h"
#include "libethercat/slave.h"
#include "libethercat/memory.h"
#include "libethercat/message_pool.h"

#include <assert.h>
#include <errno.h>
// cppcheck-suppress misra-c2012-21.10
#include <time.h>
#include <string.h>
// cppcheck-suppress misra-c2012-21.6
#include <stdio.h>
#include <pthread.h>


// get a message from a message pool
static int ec_async_message_loop_get(ec_message_pool_t *ppool,
        ec_message_entry_t **msg, ec_timer_t *timeout) {
    int ret = EC_OK;

    assert(ppool != NULL);
    assert(msg != NULL);

    if (timeout != NULL) {
        struct timespec ts = { timeout->sec, timeout->nsec };
        ret = sem_timedwait(&ppool->avail_cnt, &ts);
        int local_errno = errno;
        if (ret != 0) {
            if (local_errno != ETIMEDOUT) {
                perror("sem_timedwait");
            }

            ret = EC_ERROR_TIMEOUT;
        }
    }

    if (ret == EC_OK) {
        ret = ENODATA;
        osal_mutex_lock(&ppool->lock);

        *msg = (ec_message_entry_t *)TAILQ_FIRST(&ppool->queue);
        if ((*msg) != NULL) {
            TAILQ_REMOVE(&ppool->queue, *msg, qh);
            ret = EC_OK;
        }

        osal_mutex_unlock(&ppool->lock);
    }

    return ret;
}

// return a message to message pool
static int ec_async_message_loop_put(ec_message_pool_t *ppool, 
        ec_message_entry_t *msg) {
    assert(ppool != NULL);
    assert(msg != NULL);

    osal_mutex_lock(&ppool->lock);

    TAILQ_INSERT_TAIL(&ppool->queue, msg, qh);
    sem_post(&ppool->avail_cnt);
    
    osal_mutex_unlock(&ppool->lock);
    
    return 0;
}

// check slave expected state 
static void ec_async_check_slave(ec_async_message_loop_t *paml, uint16_t slave) {
    ec_state_t state = 0;
    uint16_t alstatcode = 0;
    
    assert(paml != NULL);
    assert(paml->pec != NULL);
    assert(slave != paml->pec->slave_cnt);

    int ret = ec_slave_get_state(paml->pec, slave, &state, &alstatcode);

    if (ret != EC_OK) {
        ec_log(10, "ec_async_thread", "slave %2d: error on "
                "getting slave state, possible slave lost, try to reconfigure\n", slave);
        
        (void)ec_set_state(paml->pec, EC_STATE_INIT);
    } else {
        // if state != expected_state -> repair
        if (state != paml->pec->slaves[slave].expected_state) {
            ec_log(10, "ec_async_thread", "slave %2d: state 0x%02X, al statuscode "
                    "0x%02X\n", slave, state, alstatcode);
            
            uint16_t wkc;
            uint8_t rx_error_counters[16];
            if (ec_fprd(paml->pec, paml->pec->slaves[slave].fixed_address, 
                0x300, &rx_error_counters[0], 16, &wkc) == 0) {

                if (wkc != 0u) {
                    int i;
                    int pos = 0;
                    char msg[128];
                    char *buf = msg;

                    for (i = 0; i < 16; ++i) {
                        pos += snprintf(&buf[pos], 128 - pos, "%02X ", rx_error_counters[i]);
                    }

                    ec_log(10, "ec_async_thread", "slave %2d: error counters %s\n", 
                            slave, msg);
                }

                ret = ec_slave_state_transition(paml->pec, slave, 
                        paml->pec->slaves[slave].expected_state);

                if (ret != EC_OK) {
                    ec_log(1, __func__, "slave %2d: ec_slave_state_transition failed!\n", slave);
                }
            }
        }
    }
}

// async loop thread
static void *ec_async_message_loop_thread(void *arg) {
    // cppcheck-suppress misra-c2012-11.5
    ec_async_message_loop_t *paml = (ec_async_message_loop_t *)arg;
    
    assert(paml != NULL);
    assert(paml->pec != NULL);

    while (paml->loop_running == 1) {
        ec_timer_t timeout;
        ec_timer_init(&timeout, 100000000 );
        ec_message_entry_t *me = NULL;

        int ret = ec_async_message_loop_get(&paml->exec, &me, &timeout);
        if (ret != EC_OK) {
            continue; // e.g. timeout
        }

        switch (me->msg.id) {
            default: 
                break;
            case EC_MSG_CHECK_GROUP: {
                // do something
                uint16_t slave;
                for (slave = 0u; slave < paml->pec->slave_cnt; ++slave) {
                    if (paml->pec->slaves[slave].assigned_pd_group != (int)me->msg.payload) {
                        continue;
                    }

                    ec_async_check_slave(paml, slave);
                }
                break;
            }
            case EC_MSG_CHECK_SLAVE:
                ec_async_check_slave(paml, me->msg.payload);
                break;
        };

        // return message to pool
        if (ec_async_message_loop_put(&paml->avail, me) == 0) {};
    }

    return NULL;
}

// execute asynchronous check group
void ec_async_check_group(ec_async_message_loop_t *paml, uint16_t gid) {
    ec_timer_t act;
    assert(paml != NULL);

    if (ec_timer_gettime(&act) == 0) {
        if (ec_timer_cmp(&act, &paml->next_check_group, <)) {
            // no need to check now
        } else {
            ec_timer_t interval = { 5, 0 };
            ec_timer_add(&act, &interval, &paml->next_check_group);

            ec_timer_t timeout;
            ec_timer_init(&timeout, 1000);
            ec_message_entry_t *me = NULL;
            int ret = ec_async_message_loop_get(&paml->avail, &me, &timeout);
            if (ret == -1) {
                // got no message buffer
            } else {
                me->msg.id = EC_MSG_CHECK_GROUP;
                me->msg.payload = gid;
                if (ec_async_message_loop_put(&paml->exec, me) == 0) {
                    ec_log(5, "ec_async_check_group", "scheduled for group %d\n", gid);
                }
            }
        }
    }
}

// creates a new async message loop
int ec_async_message_loop_create(ec_async_message_loop_t **ppaml, ec_t *pec) {
    int ret = EC_OK;
    
    assert(ppaml != NULL);
    assert(pec != NULL);

    // create memory for async message loop
    // cppcheck-suppress misra-c2012-21.3
    (*ppaml) = (ec_async_message_loop_t *)ec_malloc(sizeof(ec_async_message_loop_t));
    if (!(*ppaml)) {
        ret = ENOMEM;
    } else {
        int i = 0;

        // initiale pre-alocated async messages in avail queue
        osal_mutex_init(&(*ppaml)->avail.lock, NULL);
        osal_mutex_lock(&(*ppaml)->avail.lock);
        //(void)memset(&(*ppaml)->avail.avail_cnt, 0, sizeof(sem_t));
        sem_init(&(*ppaml)->avail.avail_cnt, 0, EC_ASYNC_MESSAGE_LOOP_COUNT);
        TAILQ_INIT(&(*ppaml)->avail.queue);

        for (i = 0; i < EC_ASYNC_MESSAGE_LOOP_COUNT; ++i) {
            // cppcheck-suppress misra-c2012-21.3
            ec_message_entry_t *me = (ec_message_entry_t *)ec_malloc(sizeof(ec_message_entry_t));
            (void)memset(me, 0, sizeof(ec_message_entry_t));
            TAILQ_INSERT_TAIL(&(*ppaml)->avail.queue, me, qh);
        }

        osal_mutex_unlock(&(*ppaml)->avail.lock);

        // initialize execute queue
        osal_mutex_init(&(*ppaml)->exec.lock, NULL);
        osal_mutex_lock(&(*ppaml)->exec.lock);
        //(void)memset(&(*ppaml)->exec.avail_cnt, 0, sizeof(sem_t));
        sem_init(&(*ppaml)->exec.avail_cnt, 0, 0);
        TAILQ_INIT(&(*ppaml)->exec.queue);
        osal_mutex_unlock(&(*ppaml)->exec.lock);

        (*ppaml)->pec = pec;
        (*ppaml)->loop_running = 1;
        if (ec_timer_gettime(&(*ppaml)->next_check_group) == 0) { 
            osal_task_create(&(*ppaml)->loop_tid, NULL, 
                    ec_async_message_loop_thread, (*ppaml));
        }
    }

    return ret;
}

// destroys async message loop
int ec_async_message_pool_destroy(ec_async_message_loop_t *paml) {
    assert(paml != NULL);
    
    // stop async thread
    paml->loop_running = 0;
    osal_task_join(&paml->loop_tid, NULL);

    // destroy message pools
    osal_mutex_lock(&paml->avail.lock);
    ec_message_entry_t *me = TAILQ_FIRST(&paml->avail.queue);
    while (me != NULL) {
        TAILQ_REMOVE(&paml->avail.queue, me, qh);
        // cppcheck-suppress misra-c2012-21.3
        ec_free(me);

        me = TAILQ_FIRST(&paml->avail.queue);
    }
    osal_mutex_unlock(&paml->avail.lock);
    osal_mutex_destroy(&paml->avail.lock);

    osal_mutex_lock(&paml->exec.lock);
    me = TAILQ_FIRST(&paml->exec.queue);
    while (me != NULL) {
        TAILQ_REMOVE(&paml->exec.queue, me, qh);
        // cppcheck-suppress misra-c2012-21.3
        ec_free(me);
    
        me = TAILQ_FIRST(&paml->exec.queue);
    }
    osal_mutex_unlock(&paml->exec.lock);
    osal_mutex_destroy(&paml->exec.lock);

    // cppcheck-suppress misra-c2012-21.3
    ec_free(paml);
    
    return 0;
}

