/**
 * \file mbx.c
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 11 Nov 2016
 *
 * \brief ethercat mailbox common access functions
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

#include "libethercat/mbx.h"
#include "libethercat/ec.h"
#include "libethercat/timer.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/eventfd.h>  

#ifndef max
#define max(a, b)  ((a) > (b) ? (a) : (b))
#endif

#define MBX_HANDLER_FLAGS_SEND  ((uint32_t)0x00000001u)
#define MBX_HANDLER_FLAGS_RECV  ((uint32_t)0x00000002u)

// forward declarations
int ec_mbx_send(ec_t *pec, uint16_t slave, uint8_t *buf, size_t buflen, uint32_t nsec);
int ec_mbx_receive(ec_t *pec, uint16_t slave, uint8_t *buf, size_t buflen, uint32_t nsec);
void ec_mbx_handler(ec_t *pec, int slave);
int ec_mbx_is_empty(ec_t *pec, uint16_t slave, uint8_t mbx_nr, uint32_t nsec);
int ec_mbx_is_full(ec_t *pec, uint16_t slave, uint8_t mbx_nr, uint32_t nsec);

//! \brief Mailbox handler thread wrapper
void *ec_mbx_handler_thread(void *arg) {
    ec_mbx_t *pmbx = (ec_mbx_t *)arg;
    ec_mbx_handler(pmbx->pec, pmbx->slave);
    return NULL;
}

//! \brief Initialize mailbox structure.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_mbx_init(ec_t *pec, uint16_t slave) {
    ec_slave_t *slv = &pec->slaves[slave];

    if (slv->mbx.handler_running) {
        return;
    }

    ec_log(10, __func__, "slave %d: initilizing mailbox\n", slave);

    pool_open(&slv->mbx.message_pool_free, 8, 1518);
    pool_open(&slv->mbx.message_pool_queued, 0, 1518);

    pthread_mutex_init(&slv->mbx.recv_mutex, NULL);
    pthread_cond_init(&slv->mbx.recv_cond, NULL);

    sem_init(&slv->mbx.sync_sem, 0, 0);
    pthread_mutex_init(&slv->mbx.lock, NULL);

    ec_coe_init(pec, slave);
    ec_soe_init(pec, slave);
    ec_foe_init(pec, slave);
    ec_eoe_init(pec, slave);

    // start mailbox handler thread
    slv->mbx.handler_running = 1;
    slv->mbx.pec = pec;
    slv->mbx.slave = slave;
    pthread_create(&slv->mbx.handler_tid, NULL, ec_mbx_handler_thread, &slv->mbx);
}

//! \brief Deinit mailbox structure
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_mbx_deinit(ec_t *pec, uint16_t slave) {
    ec_slave_t *slv = &pec->slaves[slave];

    if (!slv->mbx.handler_running) {
        return;
    }

    ec_log(10, __func__, "slave %d: deinitilizing mailbox\n", slave);

    slv->mbx.handler_running = 0;
    pthread_join(slv->mbx.handler_tid, NULL);
    
    pthread_mutex_destroy(&slv->mbx.lock);

    ec_coe_deinit(pec, slave);
    ec_soe_deinit(pec, slave);
    ec_foe_deinit(pec, slave);
    ec_eoe_deinit(pec, slave);

    pthread_mutex_destroy(&slv->mbx.recv_mutex);
    pthread_cond_destroy(&slv->mbx.recv_cond);
    
    pool_close(slv->mbx.message_pool_free);
    pool_close(slv->mbx.message_pool_queued);

}

//! \brief Check if mailbox is full.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] mbx_nr    Number of mailbox to be checked.
 * \param[in] nsec      Timeout in nanoseconds.
 * \return full (1) or empty (0)
 */
int ec_mbx_is_full(ec_t *pec, uint16_t slave, uint8_t mbx_nr, uint32_t nsec) {
    uint16_t wkc = 0;
    uint8_t sm_state = 0;

    ec_timer_t timer;
    ec_timer_init(&timer, nsec);

    do {
        ec_fprd(pec, pec->slaves[slave].fixed_address, 
                EC_REG_SM0STAT + (mbx_nr * 8), 
                &sm_state, sizeof(sm_state), &wkc);

        if (wkc && ((sm_state & 0x08) == 0x08)) 
            return 1;

        ec_sleep(EC_DEFAULT_DELAY);
    } while (!ec_timer_expired(&timer));

    return 0;
}

//! \brief Check if mailbox is empty.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] mbx_nr    Number of mailbox to be checked.
 * \param[in] nsec      Timeout in nanoseconds.
 * \return full (0) or empty (1)
 */
int ec_mbx_is_empty(ec_t *pec, uint16_t slave, uint8_t mbx_nr, uint32_t nsec) {
    uint16_t wkc = 0;
    uint8_t sm_state = 0;

    ec_timer_t timer;
    ec_timer_init(&timer, nsec);

    do {
        ec_fprd(pec, pec->slaves[slave].fixed_address, 
                EC_REG_SM0STAT + (mbx_nr * 8), 
                &sm_state, sizeof(sm_state), &wkc);

        if (wkc && ((sm_state & 0x08) == 0x00)) 
            return 1;

        ec_sleep(EC_DEFAULT_DELAY);
    } while (!ec_timer_expired(&timer));

    return 0;
}

//! \brief write mailbox to slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] nsec      Timeout in nanoseconds.
 * \return working counter
 */
int ec_mbx_send(ec_t *pec, uint16_t slave, uint8_t *buf, size_t buf_len, uint32_t nsec) {
    uint16_t wkc = 0;
    ec_slave_t *slv = &pec->slaves[slave];

    ec_timer_t timer;
    ec_timer_init(&timer, nsec);

    // wait for send mailbox available 
    if (!ec_mbx_is_empty(pec, slave, MAILBOX_WRITE, nsec)) {
        ec_log(1, __func__, "slave %d: waiting for empty send mailbox failed!\n", slave);
        return 0;
    }

    // send request
    do {
        ec_log(100, __func__, "slave %d: writing mailbox\n", slave);
        ec_fpwr(pec, slv->fixed_address, slv->sm[MAILBOX_WRITE].adr, buf, buf_len, &wkc);

        if (wkc) {
            return wkc;
        }

        ec_sleep(EC_DEFAULT_DELAY);
    } while (!ec_timer_expired(&timer));

    ec_log(1, __func__, "slave %d: did not respond on writing to write mailbox\n", slave);

    return 0;
}

//! \brief Read mailbox from slave.
/*!
 *
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] nsec      Timeout in nanoseconds.
 * \return working counter
 */
int ec_mbx_receive(ec_t *pec, uint16_t slave, uint8_t *buf, size_t buf_len, uint32_t nsec) {
    uint16_t wkc = 0;
    ec_slave_t *slv = &pec->slaves[slave];

    ec_timer_t timer;
    ec_timer_init(&timer, EC_DEFAULT_TIMEOUT_MBX);

    // wait for receive mailbox available 
    if (!ec_mbx_is_full(pec, slave, MAILBOX_READ, nsec)) {
        return 0;
    }

    // receive answer
    do {
        ec_fprd(pec, slv->fixed_address, slv->sm[MAILBOX_READ].adr,
                buf, buf_len, &wkc);

        if (wkc) {
            // reset mailbox state 
            if (slv->mbx.sm_state) {
                *slv->mbx.sm_state = 0;
            }

            return wkc;
        } else {
            // lost receive mailbox ?
            ec_log(10, __func__, "slave %d: lost receive mailbox ?\n", slave);

            uint16_t sm_status = 0;
            uint8_t sm_control = 0;
            
            ec_fprd(pec, slv->fixed_address, EC_REG_SM0STAT + 
                    (MAILBOX_READ * 8),
                    &sm_status, sizeof(sm_status), &wkc);

            sm_status ^= 0x0200; // toggle repeat request
            
            ec_fpwr(pec, slv->fixed_address, EC_REG_SM0STAT + 
                    (MAILBOX_READ * 8),
                    &sm_status, sizeof(sm_status), &wkc);

            do { 
                // wait for toggle ack
                ec_fprd(pec, slv->fixed_address, EC_REG_SM0CONTR + 
                        (MAILBOX_READ * 8),
                    &sm_control, sizeof(sm_control), &wkc);

                if (wkc && ((sm_control & 0x02) == 
                            ((sm_status & 0x0200) >> 8)))
                    break;
            } while (!ec_timer_expired(&timer) && !wkc);

            if (ec_timer_expired(&timer)) {
                ec_log(1, __func__, "slave %d timeout waiting for toggle ack\n", slave);
                return 0;
            }

            // wait for receive mailbox available 
            if (!ec_mbx_is_full(pec, slave, MAILBOX_READ, nsec)) {
                ec_log(1, __func__, "slave %d waiting for full receive "
                        "mailbox failed!\n", slave);
                return 0;
            }
        }

        ec_sleep(EC_DEFAULT_DELAY);
    } while (!ec_timer_expired(&timer));
                
    ec_log(1, __func__, "slave %d did not respond "
            "on reading from receive mailbox\n", slave);

    return 0;
}

//! \brief Enqueue mailbox message to send queue.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Entry to enqueue to be sent via mailbox.
 */
void ec_mbx_enqueue(ec_t *pec, uint16_t slave, pool_entry_t *p_entry) {
    ec_slave_t *slv = &pec->slaves[slave];

    pool_put_head(slv->mbx.message_pool_queued, p_entry);
    
    pthread_mutex_lock(&pec->slaves[slave].mbx.recv_mutex);
    slv->mbx.handler_flags |= MBX_HANDLER_FLAGS_SEND;
    
    sem_post(&slv->mbx.sync_sem);
    pthread_mutex_unlock(&pec->slaves[slave].mbx.recv_mutex);
}

//! \brief Enqueue mailbox message to send queue.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Entry to enqueue to be sent via mailbox.
 */
void ec_mbx_enqueue_tail(ec_t *pec, uint16_t slave, pool_entry_t *p_entry) {
    ec_slave_t *slv = &pec->slaves[slave];

    pool_put(slv->mbx.message_pool_queued, p_entry);
    
    pthread_mutex_lock(&pec->slaves[slave].mbx.recv_mutex);
    slv->mbx.handler_flags |= MBX_HANDLER_FLAGS_SEND;
    
    sem_post(&slv->mbx.sync_sem);
    pthread_mutex_unlock(&pec->slaves[slave].mbx.recv_mutex);
}

//! \brief Trigger read of mailbox.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 */
void ec_mbx_sched_read(ec_t *pec, uint16_t slave) {
    ec_slave_t *slv = &pec->slaves[slave];
    
    pthread_mutex_lock(&pec->slaves[slave].mbx.recv_mutex);
    slv->mbx.handler_flags |= MBX_HANDLER_FLAGS_RECV;

    pthread_cond_signal(&slv->mbx.recv_cond);
    pthread_mutex_unlock(&pec->slaves[slave].mbx.recv_mutex);
}

//! \brief Mailbox handler for one slave
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 */
void ec_mbx_handler(ec_t *pec, int slave) {
    int ret = 0, wkc;
    ec_slave_t *slv = &pec->slaves[slave];
    ec_timer_t timeout;
    pool_entry_t *p_entry = NULL;

    ec_log(10, __func__, "slave %d: started mailbox handler\n", slave);

    while (slv->mbx.handler_running) {

        // wait for mailbox event
        ec_timer_init(&timeout, 1000000);
        struct timespec ts = { timeout.sec, timeout.nsec };

        ret = sem_timedwait(&slv->mbx.sync_sem, &ts);
        
        pthread_mutex_lock(&pec->slaves[slave].mbx.recv_mutex);
        uint32_t flags = slv->mbx.handler_flags;
        slv->mbx.handler_flags = 0;
        pthread_mutex_unlock(&pec->slaves[slave].mbx.recv_mutex);

        if (!flags && (ret != 0)) {
            if ((errno == ETIMEDOUT)) {// && (slv->act_state == EC_STATE_PREOP)) {
                // check receive mailbox on timeout if PREOP or lower
                flags = MBX_HANDLER_FLAGS_RECV | MBX_HANDLER_FLAGS_SEND;
            } else {
                continue;
            }
        }

        while (flags) {
            // check event
            if (flags & MBX_HANDLER_FLAGS_RECV) {
                flags &= ~MBX_HANDLER_FLAGS_RECV;
                
                ec_log(100, __func__, "slave %d: mailbox needs to be read\n", slave);

                do {
                    pool_get(slv->mbx.message_pool_free, &p_entry, NULL);

                    if (ec_mbx_receive(pec, slave, p_entry->data, 
                                min(p_entry->data_size, slv->sm[MAILBOX_READ].len), 0)) {
                        ec_log(100, __func__, "slave %d: got one mailbox message\n", slave);

                        ec_mbx_header_t *hdr = (ec_mbx_header_t *)p_entry->data;
                        switch (hdr->mbxtype) {
                            case EC_MBX_COE:
                                ec_coe_enqueue(pec, slave, p_entry);
                                p_entry = NULL;
                                break;
                            case EC_MBX_SOE:
                                ec_soe_enqueue(pec, slave, p_entry);
                                p_entry = NULL;
                                break;
                            case EC_MBX_FOE:
                                ec_foe_enqueue(pec, slave, p_entry);
                                p_entry = NULL;
                                break;
                            case EC_MBX_EOE:
                                ec_eoe_enqueue(pec, slave, p_entry);
                                p_entry = NULL;
                                break;
                            default:
                                break;
                        }
                    }

                    if (p_entry) {
                        // returning to free pool
                        pool_put(slv->mbx.message_pool_free, p_entry);
                    }

                } while (ec_mbx_is_full(pec, slave, MAILBOX_READ, 0));
            }
            
            if (flags & MBX_HANDLER_FLAGS_SEND) {
                flags &= ~MBX_HANDLER_FLAGS_SEND;

                // need to send a message to write mailbox
                ec_log(100, __func__, "slave %d: mailbox needs to be written\n", slave);
                pool_get(slv->mbx.message_pool_queued, &p_entry, NULL);

                if (p_entry) {
                    ec_log(100, __func__, "slave %d: got mailbox buffer to write\n", slave);

                    do {
                    wkc = ec_mbx_send(pec, slave, p_entry->data, 
                            min(p_entry->data_size, slv->sm[MAILBOX_WRITE].len), EC_DEFAULT_TIMEOUT_MBX);
                    } while (!wkc);

                    if (!wkc) {
                        ec_log(1, __func__, "error on writing send mailbox -> requeue\n");
                        ec_mbx_enqueue(pec, slave, p_entry);
                    } else {                    
                        // all done
                        if (p_entry->user_cb) {
                            (*p_entry->user_cb)(p_entry->user_arg, p_entry);

                            p_entry->user_cb = NULL;
                            p_entry->user_arg = NULL;
                        }

                        pool_put(slv->mbx.message_pool_free, p_entry);
                    }
                }
            } 
        }
    }

    ec_log(10, __func__, "slave %d: stopped mailbox handler\n", slave);
}


