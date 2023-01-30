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

#include <libethercat/config.h>
#include <libethercat/hw.h>
#include <libethercat/ec.h>
#include <libethercat/idx.h>
#include <libethercat/error_codes.h>

#include <assert.h>
#include <errno.h>
#include <string.h>

#if LIBETHERCAT_HAVE_INTTYPES_H == 1
#include <inttypes.h>
#endif

// cppcheck-suppress misra-c2012-21.6
#include <stdio.h>
#include <string.h>

#if LIBETHERCAT_HAVE_ARPA_INET_H == 1
#include <arpa/inet.h>
#endif

#if LIBETHERCAT_HAVE_WINSOCK_H == 1
#include <winsock.h>
#endif

#if LIBETHERCAT_HAVE_NET_UTIL_INET_H == 1
#include <net/util/inet.h>
#endif

//! receiver thread forward declaration
static void *hw_rx_thread(void *arg);

//! open a new hw
/*!
 * \param pphw return hw 
 * \param devname ethernet device name
 * \param prio receive thread prio
 * \param cpumask receive thread cpumask
 * \return 0 or negative error code
 */
int hw_open(hw_t *phw, struct ec *pec, const osal_char_t *devname, int prio, int cpumask) {
    assert(phw != NULL);

    int ret;

    phw->pec = pec;

    (void)pool_open(&phw->tx_high, 0, NULL);
    (void)pool_open(&phw->tx_low, 0, NULL);

    osal_mutex_init(&phw->hw_lock, NULL);

    ret = hw_device_open(phw, devname);

    if (ret == EC_OK) {
        // thread settings
        phw->rxthreadprio = prio;
        phw->rxthreadcpumask = cpumask;
        phw->rxthreadrunning = 1;

        osal_task_attr_t attr;
        attr.priority = prio;
        attr.affinity = cpumask;
        (void)strcpy(&attr.task_name[0], "ecat.rx");
        osal_task_create(&phw->rxthread, &attr, hw_rx_thread, phw);
    }

    return ret;
}

//! destroys a hw
/*!
 * \param phw hw handle
 * \return 0 or negative error code
 */
int hw_close(hw_t *phw) {
    assert(phw != NULL);

    // stop receiver thread
    phw->rxthreadrunning = 0;
    osal_task_join(&phw->rxthread, NULL);

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
void hw_process_rx_frame(hw_t *phw, ec_frame_t *pframe) {
    assert(phw != NULL);
    assert(pframe != NULL);

    /* check if it is an EtherCAT frame */
    if (pframe->ethertype != htons(ETH_P_ECAT)) {
        ec_log(1, "RX_THREAD", "received non-ethercat frame! (type 0x%X)\n", pframe->type);
    } else {
        ec_datagram_t *d = ec_datagram_first(pframe); 
        while ((osal_uint8_t *) d < (osal_uint8_t *) ec_frame_end(pframe)) {
            pool_entry_t *entry = phw->tx_send[d->idx];

            if (!entry) {
                ec_log(1, "RX_THREAD", "received idx %d, but we did not send one?\n", d->idx);
            } else {
                osal_size_t size = ec_datagram_length(d);
                if (LEC_MAX_POOL_DATA_SIZE < size) {
                    ec_log(1, "RX_THREAD", "received idx %d, size %" PRIu64 " is to big for pool entry size %d!\n", 
                            d->idx, size, LEC_MAX_POOL_DATA_SIZE);
                }

                (void)memcpy(entry->data, (osal_uint8_t *)d, min(size, LEC_MAX_POOL_DATA_SIZE));

                if ((entry->user_cb) != NULL) {
                    (*entry->user_cb)(phw->pec, entry->user_arg, entry);
                }
            }
                
            d = ec_datagram_next(d);
        }
    }
}

//! receiver thread
void *hw_rx_thread(void *arg) {
    // cppcheck-suppress misra-c2012-11.5
    hw_t *phw = (hw_t *) arg;

    assert(phw != NULL);
    
    osal_task_sched_priority_t rx_prio;
    if (osal_task_get_priority(&phw->rxthread, &rx_prio) != OSAL_OK) {
        rx_prio = 0;
    }

    ec_log(10, __func__, "receive thread running (prio %d)\n", rx_prio);

    while (phw->rxthreadrunning != 0) {
        (void)hw_device_recv(phw);
    }
    
    ec_log(10, __func__, "receive thread stopped\n");
    
    return NULL;
}

//! start sending queued ethercat datagrams
/*!
 * \param phw hardware handle
 * \return 0 or error code
 */
int hw_tx(hw_t *phw) {
    assert(phw != NULL);

    int ret = EC_OK;
    ec_frame_t *pframe = NULL;

    osal_mutex_lock(&phw->hw_lock);

    (void)hw_device_get_tx_buffer(phw, &pframe);

    ec_datagram_t *pdg = ec_datagram_first(pframe);
    ec_datagram_t *pdg_prev = NULL;

    pool_t *pools[] = { &phw->tx_high, &phw->tx_low};
    int pool_idx = 0;
    pool_entry_t *p_entry = NULL;
    ec_datagram_t *p_entry_dg = NULL;

    // send high priority cyclic frames
    while ((pool_idx < 2) && (ret == EC_OK)) {
        osal_size_t len = 0;
        (void)pool_peek(pools[pool_idx], &p_entry);
        if (p_entry !=  NULL) {
            // cppcheck-suppress misra-c2012-11.3
            p_entry_dg = (ec_datagram_t *)p_entry->data;
            len = ec_datagram_length(p_entry_dg);
        }

        if (((len == 0u) && (pool_idx == 1)) || ((pframe->len + len) > phw->mtu_size)) {
            if (pframe->len == sizeof(ec_frame_t)) {
                // nothing to send
            } else {
                ret = hw_device_send(phw, pframe);
                (void)hw_device_get_tx_buffer(phw, &pframe);
                pdg = ec_datagram_first(pframe);
            }
        }

        if (len == 0u) {
            pool_idx++;
        } else {
            ret = pool_get(pools[pool_idx], &p_entry, NULL);
            if (ret == EC_OK) {
                if (pdg_prev != NULL) {
                    ec_datagram_mark_next(pdg_prev);
                }

                // cppcheck-suppress misra-c2012-11.3
                p_entry_dg = (ec_datagram_t *)p_entry->data;
                p_entry_dg->next = 0;
                (void)memcpy(pdg, p_entry_dg, ec_datagram_length(p_entry_dg));
                pframe->len += ec_datagram_length(p_entry_dg);
                pdg_prev = pdg;
                pdg = ec_datagram_next(pdg);

                // store as sent
                phw->tx_send[p_entry_dg->idx] = p_entry;
            } else {
                ret = EC_OK;
                break;
            }
        }
    }

    osal_mutex_unlock(&phw->hw_lock);

    return ret;
}

