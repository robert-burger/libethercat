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

#include "libethercat/ec.h"
#include "libethercat/error_codes.h"
#include "libethercat/slave.h"
#include "libethercat/async_loop.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
// cppcheck-suppress misra-c2012-21.6
#include <stdio.h>

// get a message from a message pool
static int ec_async_loop_get(ec_message_pool_t *ppool,
        ec_message_entry_t **msg, osal_timer_t *timeout) {
    int ret = EC_OK;

    assert(ppool != NULL);
    assert(msg != NULL);

    if (timeout != NULL) {
        osal_retval_t local_ret = osal_semaphore_timedwait(&ppool->avail_cnt, timeout);
        if (local_ret != OSAL_OK) {
            ret = EC_ERROR_UNAVAILABLE;
        }
    }

    if (ret == EC_OK) {
        ret = EC_ERROR_UNAVAILABLE;
        if (osal_mutex_lock(&ppool->lock) == OSAL_OK) {
            *msg = (ec_message_entry_t *)TAILQ_FIRST(&ppool->queue);
            if ((*msg) != NULL) {
                TAILQ_REMOVE(&ppool->queue, *msg, qh);
                ret = EC_OK;
            }

            (void)osal_mutex_unlock(&ppool->lock);
        }
    }

    return ret;
}

// return a message to message pool
static int ec_async_loop_put(ec_message_pool_t *ppool, 
        ec_message_entry_t *msg) {
    assert(ppool != NULL);
    assert(msg != NULL);

    if (osal_mutex_lock(&ppool->lock) == OSAL_OK) {
        TAILQ_INSERT_TAIL(&ppool->queue, msg, qh);
        (void)osal_semaphore_post(&ppool->avail_cnt);

        (void)osal_mutex_unlock(&ppool->lock);
    }
    
    return 0;
}

// check slave expected state 
static void ec_async_check_slave(ec_async_loop_t *paml, osal_uint16_t slave) {
    ec_state_t state = 0;
    osal_uint16_t alstatcode = 0;
    
    assert(paml != NULL);
    assert(paml->pec != NULL);
    assert(slave != paml->pec->slave_cnt);

    int ret = ec_slave_get_state(paml->pec, slave, &state, &alstatcode);

    if ((ret != EC_OK) || (state == EC_STATE_UNKNOWN)) {
        ec_state_t expected_state = paml->pec->slaves[slave].expected_state;

        ec_log(10, "ASYNC_CHECK_SLAVE", "slave %2d: error on "
                "getting slave state, possible slave lost, try to reconfigure\n", slave);

        // force it to INIT and then back to expected state
        ret = ec_slave_state_transition(paml->pec, slave, EC_STATE_INIT);

        if ((ret == EC_OK) && (EC_STATE_PREOP <= expected_state)) {
            ret = ec_slave_state_transition(paml->pec, slave, EC_STATE_PREOP);
        }

        if ((ret == EC_OK) && (EC_STATE_SAFEOP <= expected_state)) {
            ret = ec_slave_prepare_state_transition(paml->pec, slave, EC_STATE_SAFEOP);

            if (ret == EC_OK) {
                ret = ec_slave_generate_mapping(paml->pec, slave);
            }

            if (ret == EC_OK) {
                ret = ec_slave_state_transition(paml->pec, slave, EC_STATE_SAFEOP);
            }
        }

        if ((ret == EC_OK) && (EC_STATE_OP <= expected_state)) {
            ret = ec_slave_state_transition(paml->pec, slave, EC_STATE_OP);
        }

        if (ret != EC_OK) {
            ec_log(1, "ASYNC_CHECK_SLAVE", "slave %2d: ec_slave_state_transition failed!\n", slave);
        }
    } else {
        // if state != expected_state -> repair
        if (state != paml->pec->slaves[slave].expected_state) {
            ec_log(10, "ASYNC_CHECK_SLAVE", "slave %2d: state 0x%02X, al statuscode "
                    "0x%04X : %s\n", slave, state, alstatcode, al_status_code_2_string(alstatcode));
            
            osal_uint16_t wkc;
            osal_uint8_t rx_error_counters[16];
            if (ec_fprd(paml->pec, paml->pec->slaves[slave].fixed_address, 
                0x300, &rx_error_counters[0], 16, &wkc) == 0) {

                if (wkc != 0u) {
                    int i;
                    int pos = 0;
                    osal_char_t msg[128];
                    osal_char_t *buf = msg;

                    for (i = 0; i < 16; ++i) {
                        pos += snprintf(&buf[pos], 128 - pos, "%02X ", rx_error_counters[i]);
                    }

                    ec_log(10, "ASYNC_CHECK_SLAVE", "slave %2d: error counters %s\n", 
                            slave, msg);
                }

                ret = ec_slave_state_transition(paml->pec, slave, 
                        paml->pec->slaves[slave].expected_state);

                if (ret != EC_OK) {
                    ec_log(1, "ASYNC_CHECK_SLAVE", "slave %2d: ec_slave_state_transition failed!\n", slave);
                }
            }
        }
    }
}

// async loop thread
static void *ec_async_loop_thread(void *arg) {
    // cppcheck-suppress misra-c2012-11.5
    ec_async_loop_t *paml = (ec_async_loop_t *)arg;
    
    assert(paml != NULL);
    assert(paml->pec != NULL);

    ec_log(10, "ASYNC_LOOP", "async loop thread running\n");

    while (paml->loop_running == 1) {
        osal_timer_t timeout;
        osal_timer_init(&timeout, 100000000 );
        ec_message_entry_t *me = NULL;

        int ret = ec_async_loop_get(&paml->exec, &me, &timeout);
        if (ret != EC_OK) {
            continue; // e.g. timeout
        }

        switch (me->msg.id) {
            default: 
                break;
            case EC_MSG_CHECK_GROUP: {
                // do something
                osal_uint16_t slave;
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
        if (ec_async_loop_put(&paml->avail, me) == 0) {};
    }
    
    ec_log(10, "ASYNC_LOOP", "async loop thread exited\n");

    return NULL;
}

//! Execute asynchronous check all slaves.
/*!
 * \param[in] paml  Handle to async message loop.
 */
void ec_async_check_all(ec_async_loop_t *paml) {
    assert(paml != NULL);

    for (int i = 0; i < paml->pec->pd_group_cnt; ++i) {
        ec_async_check_group(paml, i);
    }
}

// execute asynchronous check group
void ec_async_check_group(ec_async_loop_t *paml, osal_uint16_t gid) {
    assert(paml != NULL);

    if (osal_timer_expired(&paml->next_check_group) == OSAL_OK) {
        // no need to check now
        ec_log(100, "ASYNC_CHECK_GROUP", "group %d: not checking now, timeout not reached\n", gid);
    } else {
        osal_timer_init(&paml->next_check_group, 1000000000);

        osal_timer_t timeout;
        osal_timer_init(&timeout, 1000);
        ec_message_entry_t *me = NULL;
        int ret = ec_async_loop_get(&paml->avail, &me, &timeout);
        if (ret == -1) {
            // got no message buffer
        } else {
            me->msg.id = EC_MSG_CHECK_GROUP;
            me->msg.payload = gid;
            if (ec_async_loop_put(&paml->exec, me) == 0) {
                ec_log(5, "ASYNC_CHECK_GROUP", "group %d: scheduled\n", gid);
            }
        }
    }
}

// creates a new async message loop
int ec_async_loop_create(ec_async_loop_t *paml, ec_t *pec) {
    int ret = EC_OK;
    
    assert(paml != NULL);
    assert(pec != NULL);

    // initiale pre-alocated async messages in avail queue
    (void)osal_mutex_init(&paml->avail.lock, NULL);
    if (osal_mutex_lock(&paml->avail.lock) == OSAL_OK) {
        int i;

        (void)osal_semaphore_init(&paml->avail.avail_cnt, 0, EC_ASYNC_MESSAGE_LOOP_COUNT);
        TAILQ_INIT(&paml->avail.queue);

        for (i = 0; i < EC_ASYNC_MESSAGE_LOOP_COUNT; ++i) {
            ec_message_entry_t *me = &paml->entries[i];
            (void)memset(me, 0, sizeof(ec_message_entry_t));
            TAILQ_INSERT_TAIL(&paml->avail.queue, me, qh);
        }

        (void)osal_mutex_unlock(&paml->avail.lock);
    }

    // initialize execute queue
    (void)osal_mutex_init(&paml->exec.lock, NULL);
    if (osal_mutex_lock(&paml->exec.lock) == OSAL_OK) {
        (void)osal_semaphore_init(&paml->exec.avail_cnt, 0, 0);
        TAILQ_INIT(&paml->exec.queue);
        (void)osal_mutex_unlock(&paml->exec.lock);
    }

    paml->pec = pec;
    paml->loop_running = 1;
    if (osal_timer_gettime(&paml->next_check_group) == 0) { 
        osal_task_attr_t attr;
        attr.priority = 0;
        attr.affinity = 0xFF;
        (void)strcpy(&attr.task_name[0], "ecat.async");
        if (osal_task_create(&paml->loop_tid, &attr, 
                ec_async_loop_thread, paml) != OSAL_OK) {
            ec_log(1, "ASYNC_INIT", "error creating async loop task!\n");
        }
    }

    return ret;
}

// destroys async message loop
int ec_async_loop_destroy(ec_async_loop_t *paml) {
    assert(paml != NULL);
    
    // stop async thread
    paml->loop_running = 0;
    (void)osal_task_join(&paml->loop_tid, NULL);

    // destroy message pools
    if (osal_mutex_lock(&paml->avail.lock) == OSAL_OK) {
        while (!TAILQ_EMPTY(&paml->avail.queue)) {
            ec_message_entry_t *me = TAILQ_FIRST(&paml->avail.queue);
            TAILQ_REMOVE(&paml->avail.queue, me, qh);
        }
        (void)osal_mutex_unlock(&paml->avail.lock);
    }
    (void)osal_mutex_destroy(&paml->avail.lock);

    if (osal_mutex_lock(&paml->exec.lock) == OSAL_OK) {
        while (!TAILQ_EMPTY(&paml->exec.queue)) {
            ec_message_entry_t *me = TAILQ_FIRST(&paml->exec.queue);
            TAILQ_REMOVE(&paml->exec.queue, me, qh);
        }
        (void)osal_mutex_unlock(&paml->exec.lock);
    }
    (void)osal_mutex_destroy(&paml->exec.lock);

    return 0;
}

