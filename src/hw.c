/**
 * \file hw.c
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief hardware access functions
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
#include <libethercat/hw.h>
#include <libethercat/ec.h>
#include <libethercat/idx.h>
#include <libethercat/error_codes.h>

#include <assert.h>
#include <errno.h>
#include <string.h>

#ifdef LIBETHERCAT_HAVE_INTTYPES_H
#include <inttypes.h>
#endif

// cppcheck-suppress misra-c2012-21.6
#include <stdio.h>
#include <string.h>

#ifdef LIBETHERCAT_HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef LIBETHERCAT_HAVE_WINSOCK_H
#include <winsock.h>
#endif

#ifdef LIBETHERCAT_HAVE_NET_UTIL_INET_H
#include <net/util/inet.h>
#endif

//! open a new hw
/*!
 * \param[in]   phw         Pointer to hw structure.
 * \param[in]   pec         Pointer to master structure.
 *
 * \return 0 or negative error code
 */
int hw_open(struct hw_common *phw, struct ec *pec) {
    assert(phw != NULL);

    int ret = EC_OK;

    phw->pec = pec;
    phw->bytes_last_sent = 0;

    (void)pool_open(&phw->tx_high, 0, NULL);
    (void)pool_open(&phw->tx_low, 0, NULL);

    osal_mutex_init(&phw->hw_lock, NULL);

    return ret;
}

//! destroys a hw
/*!
 * \param phw hw handle
 * \return 0 or negative error code
 */
int hw_close(struct hw_common *phw) {
    assert(phw != NULL);

    if (phw->close) {
        phw->close(phw);
    }

    osal_mutex_lock(&phw->hw_lock);
    (void)pool_close(&phw->tx_high);
    (void)pool_close(&phw->tx_low);

    osal_mutex_unlock(&phw->hw_lock);
    osal_mutex_destroy(&phw->hw_lock);

    return 0;
}

//! Process a received EtherCAT frame
/*!
 * \param[in]   phw     Pointer to hw handle.
 * \param[in]   pframe  Pointer to received EtherCAT frame.
 */
void hw_process_rx_frame(struct hw_common *phw, ec_frame_t *pframe) {
    assert(phw != NULL);
    assert(pframe != NULL);
    ec_t *pec = phw->pec;

    /* check if it is an EtherCAT frame */
    if (pframe->ethertype != htons(ETH_P_ECAT)) {
        ec_log(1, "HW_RX", "received non-ethercat frame! (type 0x%X)\n", pframe->type);
    } else {
        ec_datagram_t *d = ec_datagram_first(pframe); 
        while ((osal_uint8_t *) d < (osal_uint8_t *) ec_frame_end(pframe)) {
            pool_entry_t *entry = phw->tx_send[d->idx];
            phw->tx_send[d->idx] = NULL;

            if (!entry) {
                ec_log(1, "HW_RX", "received idx %d, but we did not send one?\n", d->idx);
            } else {
                if ((entry->user_cb) != NULL) {
                    (*entry->user_cb)(phw->pec, entry, d);
                }
            }

            d = ec_datagram_next(d);
        }
    }
}

//! internal tx func
static void hw_tx_pool(struct hw_common *phw, pooltype_t pool_type) {
    assert(phw != NULL);

    osal_bool_t sent = OSAL_FALSE;
    ec_frame_t *pframe = NULL;
    pool_t *pool = pool_type == POOL_HIGH ? &phw->tx_high : &phw->tx_low;

    (void)phw->get_tx_buffer(phw, &pframe);

    ec_datagram_t *pdg = ec_datagram_first(pframe);
    ec_datagram_t *pdg_prev = NULL;

    pool_entry_t *p_entry = NULL;
    ec_datagram_t *p_entry_dg = NULL;

    // send frames
    osal_size_t len;
    do {
        (void)pool_peek(pool, &p_entry);
        if (p_entry !=  NULL) {
            // cppcheck-suppress misra-c2012-11.3
            p_entry_dg = (ec_datagram_t *)p_entry->data;
            len = ec_datagram_length(p_entry_dg);
        } else { len = 0u; }

        if ((len == 0u) || ((pframe->len + len) > phw->mtu_size)) {
            if (pframe->len != sizeof(ec_frame_t)) {
                (void)phw->send(phw, pframe, pool_type);
                sent = OSAL_TRUE;
                (void)phw->get_tx_buffer(phw, &pframe);
                pdg = ec_datagram_first(pframe);
            }
        }

        if (len != 0u) {
            pool_remove(pool, p_entry);
            if (pdg_prev != NULL) {
                ec_datagram_mark_next(pdg_prev);
            }

            p_entry_dg->next = 0;
            (void)memcpy(pdg, p_entry_dg, ec_datagram_length(p_entry_dg));
            pframe->len += len;
            pdg_prev = pdg;
            pdg = ec_datagram_next(pdg);

            // store as sent
            phw->tx_send[p_entry_dg->idx] = p_entry;
        }
    } while (len > 0);
    
    if (sent == OSAL_TRUE) {
        phw->send_finished(phw);
    }
}

//! start sending queued ethercat datagrams (low prio queue)
/*!
 * \param phw hardware handle
 * \return 0 or error code
 */
int hw_tx_high(struct hw_common *phw) {
    assert(phw != NULL);

    int ret = EC_OK;

    osal_mutex_lock(&phw->hw_lock);
    osal_timer_init(&phw->next_cylce_start, phw->pec->main_cycle_interval);
    hw_tx_pool(phw, POOL_HIGH);
    osal_mutex_unlock(&phw->hw_lock);

    return ret;
}

//! start sending queued ethercat datagrams (low prio queue)
/*!
 * \param phw hardware handle
 * \return 0 or error code
 */
int hw_tx_low(struct hw_common *phw) {
    assert(phw != NULL);

    int ret = EC_OK;

    osal_mutex_lock(&phw->hw_lock);
    hw_tx_pool(phw, POOL_LOW);
    osal_mutex_unlock(&phw->hw_lock);

    return ret;
}

//! start sending queued ethercat datagrams (high and low)
/*!
 * \param phw hardware handle
 * \return 0 or error code
 */
int hw_tx(struct hw_common *phw) {
    assert(phw != NULL);

    int ret = EC_OK;

    osal_mutex_lock(&phw->hw_lock);

    osal_timer_init(&phw->next_cylce_start, phw->pec->main_cycle_interval);
    hw_tx_pool(phw, POOL_HIGH);
    hw_tx_pool(phw, POOL_LOW);

    osal_mutex_unlock(&phw->hw_lock);

    return ret;
}

