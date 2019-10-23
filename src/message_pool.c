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
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "libethercat/ec.h"
#include "libethercat/slave.h"
#include "libethercat/message_pool.h"

#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>


// get a message from a message pool
static int ec_async_message_loop_get(ec_message_pool_t *ppool,
        ec_message_entry_t **msg, ec_timer_t *timeout) {
    int ret = ENODATA;
    if (!ppool || !msg)
        return (ret = EINVAL);

    if (timeout) {
        struct timespec ts = { timeout->sec, timeout->nsec };
        ret = sem_timedwait(&ppool->avail_cnt, &ts);
        if (ret != 0) {
            if (errno != ETIMEDOUT)
                perror("sem_timedwait");

            return (ret = errno);
        }
    }

    pthread_mutex_lock(&ppool->lock);

    *msg = (ec_message_entry_t *)TAILQ_FIRST(&ppool->queue);
    if (*msg) {
        TAILQ_REMOVE(&ppool->queue, *msg, qh);
        ret = 0;
    }
    
    pthread_mutex_unlock(&ppool->lock);

    return ret;
}

// return a message to message pool
static int ec_async_message_loop_put(ec_message_pool_t *ppool, 
        ec_message_entry_t *msg) {
    int ret = 0;

    if (!ppool || !msg)
        return (ret = EINVAL);
    
    pthread_mutex_lock(&ppool->lock);

    TAILQ_INSERT_TAIL(&ppool->queue, msg, qh);
    sem_post(&ppool->avail_cnt);
    
    pthread_mutex_unlock(&ppool->lock);
    
    return 0;
}

// check slave expected state 
static void ec_async_check_slave(ec_async_message_loop_t *paml, uint16_t slave) {
    ec_state_t state = 0;
    uint16_t alstatcode = 0;
    int wkc = ec_slave_get_state(paml->pec, slave, &state, &alstatcode);

    if (!wkc) {
        ec_log(10, "ec_async_thread", "slave %2d: wkc error on "
                "getting slave state, possible slave lost, try to reconfigure\n", slave);
        
        ec_set_state(paml->pec, EC_STATE_INIT);
    } else {
        // if state != expected_state -> repair
        if (state != paml->pec->slaves[slave].expected_state) {
            ec_log(10, "ec_async_thread", "slave %2d: state 0x%02X, al statuscode "
                    "0x%02X\n", slave, state, alstatcode);
            
            uint16_t wkc2;
            uint8_t rx_error_counters[16];
            ec_fprd(paml->pec, paml->pec->slaves[slave].fixed_address, 
                0x300, &rx_error_counters[0], 16, &wkc2);

            if (wkc2) {
                int i, pos = 0;
                char msg[128];
                char *buf = msg;

                for (i = 0; i < 16; ++i)
                    pos += snprintf(buf + pos, 128 - pos, "%02X ", rx_error_counters[i]);

                ec_log(10, "ec_async_thread", "slave %2d: error counters %s\n", 
                        slave, msg);
            }

            wkc = ec_slave_state_transition(paml->pec, slave, 
                    paml->pec->slaves[slave].expected_state);
        }
    }
}

// async loop thread
static void *ec_async_message_loop_thread(void *arg) {
    ec_async_message_loop_t *paml = (ec_async_message_loop_t *)arg;

    while (paml->loop_running) {
        ec_timer_t timeout;
        ec_timer_init(&timeout, 100000000 );
        ec_message_entry_t *me = NULL;

        int ret = ec_async_message_loop_get(&paml->exec, &me, &timeout);
        if (ret != 0)
            continue; // e.g. timeout

        switch (me->msg.id) {
            default: 
                break;
            case EC_MSG_CHECK_GROUP: {
                // do something
                int slave;
                for (slave = 0; slave < paml->pec->slave_cnt; ++slave) {
                    if (paml->pec->slaves[slave].assigned_pd_group != 
                            me->msg.payload.group_id)
                        continue;

                    ec_async_check_slave(paml, slave);
                }
                break;
            }
            case EC_MSG_CHECK_SLAVE:
                ec_async_check_slave(paml, me->msg.payload.slave_id);
                break;
        };

        // return message to pool
        ec_async_message_loop_put(&paml->avail, me);
    }

    return NULL;
}

// execute asynchronous check group
void ec_async_check_group(ec_async_message_loop_t *paml, uint16_t gid) {
    ec_timer_t act;
    ec_timer_gettime(&act);
    if (ec_timer_cmp(&act, &paml->next_check_group, <)) {
        return; // no need to check now
    }

    ec_timer_t interval = { 5, 0 };
    ec_timer_add(&act, &interval, &paml->next_check_group);

    ec_timer_t timeout;
    ec_timer_init(&timeout, 1000);
    ec_message_entry_t *me = NULL;
    int ret = ec_async_message_loop_get(&paml->avail, &me, &timeout);
    if (ret == -1)
        return; // got no message buffer

    me->msg.id = EC_MSG_CHECK_GROUP;
    me->msg.payload.group_id = gid;
    ec_async_message_loop_put(&paml->exec, me);
    
    ec_log(5, "ec_async_check_group", "scheduled for group %d\n", gid);
}

// creates a new async message loop
int ec_async_message_loop_create(ec_async_message_loop_t **ppaml, ec_t *pec) {
    int i, ret = 0;
    
    // create memory for async message loop
    (*ppaml) = (ec_async_message_loop_t *)malloc(
            sizeof(ec_async_message_loop_t));
    if (!(*ppaml))
        return (ret = ENOMEM);

    // initiale pre-alocated async messages in avail queue
    pthread_mutex_init(&(*ppaml)->avail.lock, NULL);
    pthread_mutex_lock(&(*ppaml)->avail.lock);
    memset(&(*ppaml)->avail.avail_cnt, 0, sizeof(sem_t));
    sem_init(&(*ppaml)->avail.avail_cnt, 0, EC_ASYNC_MESSAGE_LOOP_COUNT);
    TAILQ_INIT(&(*ppaml)->avail.queue);
    
    for (i = 0; i < EC_ASYNC_MESSAGE_LOOP_COUNT; ++i) {
        ec_message_entry_t *me = 
            (ec_message_entry_t *)malloc(sizeof(ec_message_entry_t));
        memset(me, 0, sizeof(ec_message_entry_t));
        TAILQ_INSERT_TAIL(&(*ppaml)->avail.queue, me, qh);
    }

    pthread_mutex_unlock(&(*ppaml)->avail.lock);

    // initialize execute queue
    pthread_mutex_init(&(*ppaml)->exec.lock, NULL);
    pthread_mutex_lock(&(*ppaml)->exec.lock);
    memset(&(*ppaml)->exec.avail_cnt, 0, sizeof(sem_t));
    sem_init(&(*ppaml)->exec.avail_cnt, 0, 0);
    TAILQ_INIT(&(*ppaml)->exec.queue);
    pthread_mutex_unlock(&(*ppaml)->exec.lock);

    (*ppaml)->pec = pec;
    (*ppaml)->loop_running = 1;
    ec_timer_gettime(&(*ppaml)->next_check_group);
    pthread_create(&(*ppaml)->loop_tid, NULL, 
            ec_async_message_loop_thread, (*ppaml));

    return 0;
}

// destroys async message loop
int ec_async_message_pool_destroy(ec_async_message_loop_t *paml) {
    if (!paml)
        return EINVAL;
    
    // stop async thread
    paml->loop_running = 0;
    pthread_join(paml->loop_tid, NULL);

    // destroy message pools
    ec_message_entry_t *me;
    pthread_mutex_lock(&paml->avail.lock);
    while ((me = TAILQ_FIRST(&paml->avail.queue)) != NULL) {
        TAILQ_REMOVE(&paml->avail.queue, me, qh);
        free(me);
    }
    pthread_mutex_unlock(&paml->avail.lock);
    pthread_mutex_destroy(&paml->avail.lock);

    pthread_mutex_lock(&paml->exec.lock);
    while ((me = TAILQ_FIRST(&paml->exec.queue)) != NULL) {
        TAILQ_REMOVE(&paml->exec.queue, me, qh);
        free(me);
    }
    pthread_mutex_unlock(&paml->exec.lock);
    pthread_mutex_destroy(&paml->exec.lock);

    free(paml);
    
    return 0;
}

