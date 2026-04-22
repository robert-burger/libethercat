/**
 * \file mbx_gateway.c
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 11 Apr 2026
 *
 * \brief ethercat mailbox gateway functions
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

#include <string.h>

#include "libethercat/mbx_gateway.h"
#include "libethercat/pool.h"
#include "libethercat/ec.h"
#include "libethercat/mbx.h"
#include "libethercat/error_codes.h"

//! Initialize MBX Gateway structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 */
void ec_mbx_gateway_init(struct ec *pec) {
    (void)pool_open(&pec->mbx_gw_recv_pool, 0, NULL);
}

//! deinitialize MBX Gateway structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 */
void ec_mbx_gateway_deinit(struct ec *pec) {
    (void)pool_close(&pec->mbx_gw_recv_pool);
}

//! \brief Wait for MBX Gateway message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] pp_entry  Returns pointer to pool entry containing received
 *                      mailbox message from slave.
 */
static void ec_mbx_gateway_wait(ec_t *pec, pool_entry_t **pp_entry) {
    assert(pec != NULL);
    assert(pp_entry != NULL);
    *pp_entry = NULL;

    osal_timer_t timeout;
    osal_timer_t timeout_loop;
    osal_timer_init(&timeout_loop, (osal_int64_t)EC_DEFAULT_TIMEOUT_MBX*10);

    do {
        osal_timer_init(&timeout, 100000);
        (void)pool_get(&pec->mbx_gw_recv_pool, pp_entry, &timeout);
    } while ((osal_timer_expired(&timeout_loop) == OSAL_OK) && (*pp_entry == NULL));
}

//! \brief Enqueue MBX Gateway message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] p_entry   Pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_mbx_gateway_enqueue(ec_t *pec, pool_entry_t *p_entry) {
    assert(pec != NULL);
    assert(p_entry != NULL);

    pool_put(&pec->mbx_gw_recv_pool, p_entry);
}

//! \brief Handle a mailbox gateway request.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in,out] echdr Pointer to EC-header of mailbox gateway request.
 *                      Response is returned here.
 * \param[in] len       Lenght of echdr buffer.
 *
 * \return EC_OK on success.
 */
int ec_mbx_gateway_handle(struct ec *pec, struct echdr *echdr, size_t len) {
    struct ec_mbx_header *mbxhdr = (struct ec_mbx_header *)((uint8_t *)echdr + 2);
    (void)len;

    if (mbxhdr->address == 0) {
        // addressed to master
        if (mbxhdr->mbxtype == EC_MBX_COE) {
            ec_coe_header_t *coehdr = (ec_coe_header_t *)((uint8_t *)mbxhdr + sizeof(struct ec_mbx_header));
            if (coehdr->service == EC_COE_SDOREQ) {
                ec_sdo_init_download_header_t *sdohdr = (ec_sdo_init_download_header_t *)(
                        (uint8_t *)coehdr + sizeof(ec_coe_header_t));

                osal_uint8_t tmpbuf[256];
                osal_size_t tmpbuf_len = 256;
                osal_uint32_t abort_code = 0;
                int ret = ec_coe_master_sdo_read(pec, sdohdr->index, sdohdr->sub_index, sdohdr->complete,
                        &tmpbuf[0], &tmpbuf_len, &abort_code);
                if (ret != EC_OK) { ec_log(1, "MBX_GATEWAY", "ec_coe_master_sdo_read return error: %d\n", ret); }

                coehdr->service = EC_COE_SDORES;
                sdohdr->command = EC_COE_SDO_UPLOAD_REQ;

                uint8_t *coepayload = ((uint8_t *)sdohdr + sizeof(ec_sdo_init_download_header_t));

                if (tmpbuf_len <= 4) {
                    sdohdr->transfer_type = 1;
                    sdohdr->data_set_size = 4 - tmpbuf_len;
                } else {
                    sdohdr->size_indicator = 1;
                    sdohdr->transfer_type = 0;
                    sdohdr->data_set_size = 0;
                    *(uint32_t *)coepayload = tmpbuf_len;
                    coepayload += 4;

                    mbxhdr->length += tmpbuf_len;
                    echdr->length = echdr->length + tmpbuf_len;
                }

                memcpy(coepayload, tmpbuf, tmpbuf_len);
            }
        } 
    } else {
        for (int slave = 0; slave < pec->slave_cnt; ++slave) {
            if (pec->slaves[slave].fixed_address == mbxhdr->address) {
                int mbx_ret = 0;
                pool_entry_t *p_entry = NULL;

                if (ec_mbx_get_free_send_buffer(pec, slave, &p_entry, NULL) != 0) {
                    ec_log(1, "MBX_GATEWAY", "error getting free send buffer\n");
                    mbx_ret = EC_ERROR_MAILBOX_OUT_OF_SEND_BUFFERS;
                } else {
                    int counter;
                    (void)ec_mbx_next_counter(pec, slave, &counter);

                    mbxhdr->address |= 0x8000;
                    mbxhdr->counter = counter;
                    memcpy(p_entry->data, mbxhdr, (*(uint16_t *)echdr) & 0x07FF);

                    // send request
                    ec_mbx_enqueue_head(pec, slave, p_entry);

                    // wait for answer
                    ec_mbx_gateway_wait(pec, &p_entry);
                    while (p_entry != NULL) {
                        struct ec_mbx_header *mbxhdr2 = (struct ec_mbx_header *)(p_entry->data);
                        memcpy(mbxhdr, p_entry->data, mbxhdr2->length + sizeof(struct ec_mbx_header));
                        mbxhdr->address &= ~0x8000;

                        echdr->length = mbxhdr->length + sizeof(struct ec_mbx_header);

                        ec_mbx_return_free_recv_buffer(pec, p_entry);
                        break;
                    }
                }

                (void)mbx_ret;

                break;
            }
        }
    }

    return EC_OK;
}

