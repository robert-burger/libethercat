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

void ec_mbx_handler(ec_t *pec, int slave);

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

    ec_index_init(&slv->mbx.idx_q, 8);
    pool_open(&slv->mbx.message_pool_free, 8, 1518);
    pool_open(&slv->mbx.message_pool_queued, 0, 1518);

    pthread_mutex_init(&slv->mbx.recv_mutex, NULL);
    pthread_cond_init(&slv->mbx.recv_cond, NULL);

    slv->mbx.pec = pec;
    slv->mbx.slave = slave;

    pthread_create(&slv->mbx.recv_tid, NULL, ec_mbx_handler_thread, &slv->mbx);
}

//! check if mailbox is full
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param mbx_nr number of mailbox
 * \param nsec timeout in nanoseconds
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

//! check if mailbox is empty
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param mbx_nr number of mailbox
 * \param nsec timeout in nanoseconds
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

//! clears mailbox buffers 
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param read read mailbox (1) or write mailbox (0)
 */
void ec_mbx_clear(ec_t *pec, uint16_t slave, int read) {
    ec_slave_t *slv = &pec->slaves[slave];

    if (read)
        memset(slv->mbx_read.buf, 0, slv->sm[slv->mbx_read.sm_nr].len);
    else
        memset(slv->mbx_write.buf, 0, slv->sm[slv->mbx_write.sm_nr].len);
}

//! write mailbox to slave
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param nsec timeout in nanoseconds
 * \return working counter
 */
int ec_mbx_send(ec_t *pec, uint16_t slave, uint32_t nsec) {
    uint16_t wkc = 0;
    ec_slave_t *slv = &pec->slaves[slave];

    if (!slv->sm[slv->mbx_write.sm_nr].len) {
        ec_log(1, __func__, "write mailbox on slave %d not available\n", 
                slave);
        return 0;
    }
    
    ec_timer_t timer;
    ec_timer_init(&timer, nsec);

    // wait for send mailbox available 
    if (!ec_mbx_is_empty(pec, slave, slv->mbx_write.sm_nr, nsec)) {
        ec_log(1, __func__, "slave %d waiting for empty send "
                "mailbox failed!\n", slave);
        return 0;
    }

    // send request
    do {
        ec_fpwr(pec, slv->fixed_address, slv->sm[slv->mbx_write.sm_nr].adr, 
                slv->mbx_write.buf, slv->sm[slv->mbx_write.sm_nr].len, &wkc);

        if (wkc)
            return wkc;

        ec_sleep(EC_DEFAULT_DELAY);
    } while (!ec_timer_expired(&timer));

    ec_log(1, __func__, "slave %d did not respond "
            "on writing to write mailbox\n", slave);

    return 0;
}

//! read mailbox from slave
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \param nsec timeout in nanoseconds
 * \return working counter
 */
int ec_mbx_receive(ec_t *pec, uint16_t slave, uint32_t nsec) {
    uint16_t wkc = 0;
    ec_slave_t *slv = &pec->slaves[slave];

    if (!slv->sm[slv->mbx_read.sm_nr].len)
        return 0;

    ec_timer_t timer;
    ec_timer_init(&timer, EC_DEFAULT_TIMEOUT_MBX);

    // wait for receive mailbox available 
    if (!ec_mbx_is_full(pec, slave, slv->mbx_read.sm_nr, nsec)) {
        return 0;
    }

    // receive answer
    do {
        ec_fprd(pec, slv->fixed_address, slv->sm[slv->mbx_read.sm_nr].adr,
                slv->mbx_read.buf, slv->sm[slv->mbx_read.sm_nr].len, &wkc);

        if (wkc)
            return wkc;
        else {
            // lost receive mailbox ?
            uint16_t sm_status = 0;
            uint8_t sm_control = 0;
            
            ec_fprd(pec, slv->fixed_address, EC_REG_SM0STAT + 
                    (slv->mbx_read.sm_nr * 8),
                    &sm_status, sizeof(sm_status), &wkc);

            sm_status ^= 0x0200; // toggle repeat request
            
            ec_fpwr(pec, slv->fixed_address, EC_REG_SM0STAT + 
                    (slv->mbx_read.sm_nr * 8),
                    &sm_status, sizeof(sm_status), &wkc);

            do { 
                // wait for toggle ack
                ec_fprd(pec, slv->fixed_address, EC_REG_SM0CONTR + 
                        (slv->mbx_read.sm_nr * 8),
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
            if (!ec_mbx_is_full(pec, slave, slv->mbx_read.sm_nr, nsec)) {
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

//! push current received mailbox to received queue
/*!
 * \param[in] pec pointer to ethercat master
 * \param[in] slave slave number
 */
void ec_mbx_push(ec_t *pec, uint16_t slave) {
}

//! \brief Push current received mailbox to received queue.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Entry to enqueue to be sent via mailbox.
 */
void ec_mbx_enqueue(ec_t *pec, uint16_t slave, pool_entry_t *p_entry) {
    pool_put(pec->slaves[slave].mbx.message_pool_queued, p_entry);

    pthread_mutex_lock(&pec->slaves[slave].mbx.recv_mutex);
    pthread_cond_signal(&pec->slaves[slave].mbx.recv_cond);
    pthread_mutex_unlock(&pec->slaves[slave].mbx.recv_mutex);
}

void ec_mbx_handler(ec_t *pec, int slave) {
    int ret, wkc;
    ec_slave_t *slv = &pec->slaves[slave];
    ec_timer_t timeout;

    printf("MBX HANDLER running for slave %d\n", slave);
    pthread_mutex_lock(&pec->slaves[slave].mbx.recv_mutex);

    while (1) {
        ec_timer_init(&timeout, 1000000);
        struct timespec ts = { timeout.sec, timeout.nsec };

        ret = pthread_cond_timedwait(&slv->mbx.recv_cond, &slv->mbx.recv_mutex, &ts);

        if (ret != 0) {
            continue;
        }

        // need to send a message to write mailbox
        printf("mailbox of slave %d needs to be written\n", slave);

        pool_entry_t *p_entry = NULL;
        pool_get(slv->mbx.message_pool_queued, &p_entry, NULL);

        while (p_entry) {
            ec_mbx_clear(pec, slave, 0);
            memcpy(slv->mbx_write.buf, p_entry->data, 
                    min(p_entry->data_size, slv->sm[slv->mbx_write.sm_nr].len));
            
            // returning to free pool
            pool_put(slv->mbx.message_pool_free, p_entry);

            wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
            if (!wkc) {
                ec_log(1, __func__, "error on writing send mailbox\n");
            }

            // get next
            pool_get(slv->mbx.message_pool_queued, &p_entry, NULL);
        }
        

//        wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
//        if (!wkc) {
//            ec_log(1, __func__, "error on writing send mailbox\n");
//        }
//
        printf("sending done\n");
    }
    
    pthread_mutex_unlock(&pec->slaves[slave].mbx.recv_mutex);
    printf("MBX HANDLER stopped for slave %d\n", slave);
}


