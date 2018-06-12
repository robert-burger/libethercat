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
        ec_log(10, __func__, "write mailbox on slave %d not available\n", 
                slave);
        return 0;
    }
    
    ec_timer_t timer;
    ec_timer_init(&timer, nsec);

    // wait for send mailbox available 
    if (!ec_mbx_is_empty(pec, slave, slv->mbx_write.sm_nr, nsec)) {
        ec_log(10, __func__, "slave %d waiting for empty send "
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

    ec_log(10, __func__, "slave %d did not respond "
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

            if (ec_timer_expired(&timer))
                return 0;

            // wait for receive mailbox available 
            if (!ec_mbx_is_full(pec, slave, slv->mbx_read.sm_nr, nsec)) {
                ec_log(10, __func__, "slave %d waiting for full receive "
                        "mailbox failed!\n", slave);
                return 0;
            }
        }

        ec_sleep(EC_DEFAULT_DELAY);
    } while (!ec_timer_expired(&timer));
                
    ec_log(10, __func__, "slave %d did not respond "
            "on reading from receive mailbox\n", slave);

    return 0;
}

//! queue read mailbox content
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 */
void ec_mbx_queue(ec_t *pec, uint16_t slave) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    // get length and queue this message    
    ec_mbx_header_t *hdr = (ec_mbx_header_t *)slv->mbx_read.buf;
    size_t read_len = 6 + hdr->length;
    ec_queued_mailbox_message_entry_t *qmsg = (ec_queued_mailbox_message_entry_t *)
        malloc(sizeof(ec_queued_mailbox_message_entry_t) + read_len);
    memcpy(qmsg->msg, slv->mbx_read.buf, read_len);
    TAILQ_INSERT_TAIL(&slv->mbx_queue, qmsg, qh);
}

