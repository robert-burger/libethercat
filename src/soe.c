/**
 * \file soe.c
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 25 Nov 2016
 *
 * \brief ethercat soe functions.
 *
 * Implementaion of the Servodrive over EtherCAT mailbox protocol
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

#include "libethercat/slave.h"
#include "libethercat/ec.h"
#include "libethercat/soe.h"
#include "libethercat/error_codes.h"

#include <assert.h>
// cppcheck-suppress misra-c2012-21.6
#include <stdio.h>
#include <string.h>

#if LIBETHERCAT_HAVE_INTTYPES_H == 1
#include <inttypes.h>
#endif


//! SoE mailbox structure
typedef struct PACKED ec_soe_header {
    osal_uint8_t op_code    : 3;         //!< op code
    osal_uint8_t incomplete : 1;         //!< incompletion flag
    osal_uint8_t error      : 1;         //!< error flag
    osal_uint8_t atn        : 3;         //!< at number
    osal_uint8_t elements;               //!< servodrive element mask
    osal_uint16_t idn_fragments_left;    //!< fragments left
} PACKED ec_soe_header_t;

typedef struct PACKED ec_soe_request {
    ec_mbx_header_t mbx_hdr;
    ec_soe_header_t soe_hdr;
    ec_data_t       data;
} PACKED ec_soe_request_t;

//! soe op codes
enum {
    EC_SOE_READ_REQ     = 0x01,
    EC_SOE_READ_RES,
    EC_SOE_WRITE_REQ,
    EC_SOE_WRITE_RES,
    EC_SOE_NOTIFICATION,
    EC_SOE_EMERGENCY
};

#define SOE_LOG_BUF_SIZE    1024u
static char soe_log_buf[SOE_LOG_BUF_SIZE];

//! initialize SoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_soe_init(ec_t *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    if (pool_open(&slv->mbx.soe.recv_pool, 0, NULL) != EC_OK) {
        ec_log(1, "SOE_INIT", "pool_open failed!\n");
    }
}

//! deinitialize SoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_soe_deinit(ec_t *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    if (pool_close(&slv->mbx.soe.recv_pool) != EC_OK) {
        ec_log(1, "SOE_DEINIT", "pool_close failed!\n");
    }
}

//! \brief Wait for SoE message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] pp_entry  Returns pointer to pool entry containing received
 *                      mailbox message from slave.
 */
static void ec_soe_wait(ec_t *pec, osal_uint16_t slave, pool_entry_t **pp_entry) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(pp_entry != NULL);

    ec_slave_ptr(slv, pec, slave);

    ec_mbx_sched_read(pec, slave);

    osal_timer_t timeout;
    osal_timer_init(&timeout, EC_DEFAULT_TIMEOUT_MBX);

    // Do not care about return value here. In this case there are no new SoE messages.
    (void)pool_get(&slv->mbx.soe.recv_pool, pp_entry, &timeout);
}

//! \brief Enqueue SoE message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_soe_enqueue(ec_t *pec, osal_uint16_t slave, pool_entry_t *p_entry) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(p_entry != NULL);

    ec_slave_ptr(slv, pec, slave);
        
    // cppcheck-suppress misra-c2012-11.3
    ec_mbx_header_t *mbx_hdr = (ec_mbx_header_t *)(p_entry->data);
    // cppcheck-suppress misra-c2012-11.3
    ec_soe_header_t *soe_hdr = (ec_soe_header_t *)(&p_entry->data[sizeof(ec_mbx_header_t)]); 

    // check for correct op_code
    if (soe_hdr->op_code == EC_SOE_NOTIFICATION) {
        int local_ret;

        local_ret = snprintf(soe_log_buf, SOE_LOG_BUF_SIZE, "SoE Notification: opcode %d, incomplete %d, error %d, "
                "atn %d, elements %d, idn %d", soe_hdr->op_code, soe_hdr->incomplete, soe_hdr->error, 
                soe_hdr->atn, soe_hdr->elements, soe_hdr->idn_fragments_left);
        if (local_ret >= 0) {
            osal_uint32_t soe_log_pos = (osal_uint32_t)local_ret;

            if ((mbx_hdr->length - sizeof(ec_soe_header_t)) > 0u) {
                local_ret = snprintf(&soe_log_buf[soe_log_pos], SOE_LOG_BUF_SIZE - soe_log_pos, ", payload: ");

                if (local_ret >= 0) {
                    soe_log_pos += (osal_uint32_t)local_ret;

                    osal_uint8_t *payload = (osal_uint8_t *)(&p_entry->data[sizeof(ec_mbx_header_t) + sizeof(ec_soe_header_t)]);

                    for (osal_uint32_t i = 0; i < (mbx_hdr->length - sizeof(ec_soe_header_t)); ++i) {
                        local_ret = snprintf(&soe_log_buf[soe_log_pos], SOE_LOG_BUF_SIZE - soe_log_pos, "%02X", payload[i]);
                        if (local_ret < 0) {
                            break;
                        }

                        soe_log_pos += (osal_uint32_t)local_ret;
                    }
                }
            }
        } 

        if (local_ret < 0) {
            ec_log(1, "SOE_ENQUEUE", "snprintf failed with %d\n", local_ret);
        }
        
        ec_log(10, "SOE_ENQUEUE", "%s\n", soe_log_buf);

        ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
    } else {
        pool_put(&slv->mbx.soe.recv_pool, p_entry);
    }
}

//! Read elements of soe ID number
/*!
 * This reads all ID number elements from given EtherCAT slave's drive number. This function
 * enables read access to the ServoDrive dictionary on SoE enabled devices. The call to 
 * \link ec_soe_read \endlink is a synchronous call and will block until it's either finished or aborted.
 *
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] atn       AT number according to ServoDrive Bus Specification. 
 *                      In most cases this will be 0, if you are using a 
 *                      multi-drive device this may be 0,1,... .
 * \param[in] idn       ID number according to ServoDrive Bus Specification.
 *                      IDN's in range 1...32767 are described in the Specification,
 *                      IDN's greater 32768 are manufacturer specific.
 * \param[in] elements  ServoDrive elements according to ServoDrive Bus Specification.
 * \param[in,out] buf   Buffer for where to store the answer. 
 * \param[in,out] len   Length of \p buf, see 'buf' descriptions. Returns length of answer.
 * \return EC_OK on successs, EC_ERROR_MAILBOX_* otherwise.
 */
int ec_soe_read(ec_t *pec, osal_uint16_t slave, osal_uint8_t atn, osal_uint16_t idn, 
        osal_uint8_t *elements, osal_uint8_t *buf, osal_size_t *len) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(buf != NULL);
    assert(len != NULL);

    int ret = EC_ERROR_MAILBOX_TIMEOUT;
    int counter;
    ec_slave_ptr(slv, pec, slave);
    pool_entry_t *p_entry;
    
    osal_mutex_lock(&slv->mbx.soe.lock);

    if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_SOE) != EC_OK) {
        ret = EC_ERROR_MAILBOX_NOT_SUPPORTED_SOE;
    } else if (ec_mbx_get_free_send_buffer(pec, slave, &p_entry, NULL) != EC_OK) {
        ret = EC_ERROR_MAILBOX_OUT_OF_WRITE_BUFFERS;
    } else {
        // cppcheck-suppress misra-c2012-11.3
        ec_soe_request_t *write_buf = (ec_soe_request_t *)(p_entry->data);

        (void)ec_mbx_next_counter(pec, slave, &counter);

        // mailbox header
        write_buf->mbx_hdr.length   = sizeof(ec_soe_header_t);
        write_buf->mbx_hdr.mbxtype  = EC_MBX_SOE;
        write_buf->mbx_hdr.counter  = counter;
        // soe header
        write_buf->soe_hdr.op_code    = EC_SOE_READ_REQ;
        write_buf->soe_hdr.atn        = atn;
        write_buf->soe_hdr.elements   = *elements;
        write_buf->soe_hdr.idn_fragments_left = idn;

        // send request
        ec_mbx_enqueue_head(pec, slave, p_entry);

        osal_uint8_t *to = buf;
        osal_ssize_t left_len = *len;
        osal_uint8_t first_fragment = 1u;

        // wait for answer
        ec_soe_wait(pec, slave, &p_entry);
        while (p_entry != NULL) {
            // cppcheck-suppress misra-c2012-11.3
            ec_soe_request_t *read_buf  = (ec_soe_request_t *)(p_entry->data); 

            // check for correct op_code
            if (read_buf->soe_hdr.op_code != EC_SOE_READ_RES) {
                ec_log(5, "SOE_READ", "got unexpected response: opcode %d, incomplete %d, error %d, "
                        "atn %d, elements %d, idn %d\n", read_buf->soe_hdr.op_code, read_buf->soe_hdr.incomplete, read_buf->soe_hdr.error, 
                        read_buf->soe_hdr.atn, read_buf->soe_hdr.elements, read_buf->soe_hdr.idn_fragments_left);
                ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
                continue; // TODO handle unexpected answer
            }

            if (first_fragment == 1u) {
                // return elements we got from slave
                *elements = read_buf->soe_hdr.elements;

                if (left_len == 0) {
                    *len = read_buf->mbx_hdr.length - sizeof(ec_soe_header_t);

                    if (read_buf->soe_hdr.idn_fragments_left != 0u) {
                        *len = (read_buf->soe_hdr.idn_fragments_left + 1u) * (*len);
                        left_len = *len;
                    }
                }

                first_fragment = 0;
            }

            if (left_len > 0) {
                osal_size_t read_len = read_buf->mbx_hdr.length - sizeof(ec_soe_header_t);
                (void)memcpy(to, &read_buf->data, min(read_len, (osal_size_t)left_len));
                to = &to[read_len];
                left_len -= (osal_ssize_t)read_len;
            }

            ec_mbx_return_free_recv_buffer(pec, slave, p_entry);

            if (/*(left_len < 0) ||*/ !read_buf->soe_hdr.incomplete) {
                ret = EC_OK;
                break;            
            }
        
            ec_soe_wait(pec, slave, &p_entry);
        }
    }
    
    osal_mutex_unlock(&slv->mbx.soe.lock);
    
    return ret;
}

int ec_soe_write(ec_t *pec, osal_uint16_t slave, osal_uint8_t atn, osal_uint16_t idn, 
        osal_uint8_t elements, osal_uint8_t *buf, osal_size_t len) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(buf != NULL);

    int ret = EC_ERROR_MAILBOX_TIMEOUT;
    int counter;
    ec_slave_ptr(slv, pec, slave);
    pool_entry_t *p_entry;

    osal_mutex_lock(&slv->mbx.soe.lock);

    if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_SOE) != EC_OK) {
        ret = EC_ERROR_MAILBOX_NOT_SUPPORTED_SOE;
    } else {
        osal_uint8_t *from = buf;
        osal_size_t left_len = len;
        osal_size_t mbx_len = slv->sm[MAILBOX_WRITE].len 
            - sizeof(ec_mbx_header_t) - sizeof(ec_soe_header_t);

        ec_log(100, "SOE_WRITE", "slave %d, atn %d, idn %d, elements %d, buf %p, "
                "len %" PRIu64 ", left %" PRIu64 ", mbx_len %" PRIu64 "\n", 
                slave, atn, idn, elements, buf, len, left_len, mbx_len);

        osal_uint32_t soe_log_pos = 0;

        for (osal_uint32_t i = 0; i < len; ++i) {
            int local_ret = snprintf(&soe_log_buf[soe_log_pos], SOE_LOG_BUF_SIZE - soe_log_pos, "%02X", buf[i]);
            if (local_ret >= 0) {
                soe_log_pos += (osal_uint32_t)local_ret;
            } else {
                ec_log(1, "SOE_WRITE", "snprintf failed with %d!\n", local_ret);
            }
        }

        ec_log(100, "SOE_WRITE", "%s\n", soe_log_buf);

        while ((left_len != 0u) && (ret != EC_ERROR_MAILBOX_OUT_OF_WRITE_BUFFERS)) {
            if (ec_mbx_get_free_send_buffer(pec, slave, &p_entry, NULL) != EC_OK) {
                ret = EC_ERROR_MAILBOX_OUT_OF_WRITE_BUFFERS;
            } else {
                // cppcheck-suppress misra-c2012-11.3
                ec_soe_request_t *write_buf = (ec_soe_request_t *)(p_entry->data);

                (void)ec_mbx_next_counter(pec, slave, &counter);

                // mailbox header
                write_buf->mbx_hdr.length   = sizeof(ec_soe_header_t);
                write_buf->mbx_hdr.mbxtype  = EC_MBX_SOE;
                write_buf->mbx_hdr.counter  = counter;
                // soe header
                write_buf->soe_hdr.op_code    = EC_SOE_WRITE_REQ;
                write_buf->soe_hdr.atn        = atn;
                write_buf->soe_hdr.elements   = elements;
                write_buf->soe_hdr.idn_fragments_left = idn;

                osal_size_t send_len = min(left_len, mbx_len);
                write_buf->mbx_hdr.length = sizeof(ec_soe_header_t) + send_len;
                (void)memcpy(&write_buf->data, from, send_len);
                from = &from[send_len];
                left_len -= send_len;

                ec_log(100, "SOE_WRITE", "slave %d, atn %d, idn %d, elements %d: sending fragment len %" PRIu64 " (left %" PRIu64 ")\n",
                        slave, atn, idn, elements, send_len, left_len);

                if (left_len != 0u) {
                    write_buf->soe_hdr.incomplete = 1;
                    write_buf->soe_hdr.idn_fragments_left = ((osal_size_t)left_len / mbx_len) + 1u;
                } else {
                    write_buf->soe_hdr.incomplete = 0;
                    write_buf->soe_hdr.idn_fragments_left = idn;
                }

                // send request
                ec_mbx_enqueue_head(pec, slave, p_entry);

                // wait for answer
                ec_soe_wait(pec, slave, &p_entry);
                while (p_entry != NULL) {
                    // cppcheck-suppress misra-c2012-11.3
                    ec_soe_request_t *read_buf  = (ec_soe_request_t *)(p_entry->data);                 

                    ec_log(100, "SOE_WRITE", "got response: opcode %d, incomplete %d, error %d, "
                            "atn %d, elements %d, idn %d\n", read_buf->soe_hdr.op_code, read_buf->soe_hdr.incomplete, read_buf->soe_hdr.error, 
                            read_buf->soe_hdr.atn, read_buf->soe_hdr.elements, read_buf->soe_hdr.idn_fragments_left);

                    osal_uint8_t op_code = read_buf->soe_hdr.op_code;
                    ec_mbx_return_free_recv_buffer(pec, slave, p_entry);

                    // check for correct op_code
                    if (op_code != EC_SOE_WRITE_RES) {
                        ec_log(5, "SOE_WRITE", "got unexpected response %d\n", op_code);
                    } else {
                        ret = EC_OK;
                        break;
                    }

                    ec_soe_wait(pec, slave, &p_entry);
                }
            }
        }
    }

    osal_mutex_unlock(&slv->mbx.soe.lock);

    return ret;
}

static int ec_soe_generate_mapping_local(ec_t *pec, osal_uint16_t slave, osal_uint8_t atn, 
        osal_uint16_t idn, osal_uint32_t *bitsize) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(bitsize != NULL);
    
    int ret = EC_OK;
    osal_uint16_t idn_value[512];
    osal_uint8_t elements; 

    *bitsize = 16u; // control and status word are always present

    // read size of mapping idn first
    osal_uint16_t idn_len[2];
    osal_uint8_t *buf = (osal_uint8_t *)&idn_len[0];
    osal_size_t idn_len_size = sizeof(idn_len);
    elements = EC_SOE_VALUE;

    ret = ec_soe_read(pec, slave, atn, idn, &elements, buf, &idn_len_size);

    if (ret == EC_OK) {
        // read mapping idn
        osal_size_t idn_size = min((idn_len[0] + 4u), 512);
        elements = EC_SOE_VALUE;
        ret = ec_soe_read(pec, slave, atn, idn, &elements, (osal_uint8_t *)&idn_value[0], &idn_size);
    }

    if (ret == EC_OK) {
        osal_uint16_t i;

        // read all mapped idn's and add bit length, 
        // length is stored in idn attributes
        for (i = 0u; i < (idn_len[0]/2u); ++i) {
            osal_uint16_t sub_idn = idn_value[i+2u];
            ec_soe_idn_attribute_t sub_idn_attr;
            buf = (osal_uint8_t *)&sub_idn_attr;
            osal_size_t sub_idn_attr_size = sizeof(sub_idn_attr);
            elements = EC_SOE_ATTRIBUTE;

            ec_log(100, "SOE_MAPPING", "atn %d, read mapped idn %d\n", atn, sub_idn);

            if (ec_soe_read(pec, slave, atn, sub_idn, &elements, 
                        buf, &sub_idn_attr_size) != EC_OK) {
                continue;
            }

            // 0 = 8 bit, 1 = 16 bit, ...
            *bitsize += 8u << sub_idn_attr.length;
        }

        ec_log(10, "SOE_MAPPING", "soe mapping for idn %d, bitsize %u\n", idn, *bitsize);
    }

    return ret;
}

int ec_soe_generate_mapping(ec_t *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    int ret = EC_OK;

    if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_SOE) != EC_OK) {
        ret = EC_ERROR_MAILBOX_NOT_SUPPORTED_SOE;
    } else {
        osal_uint8_t atn;
        const osal_uint16_t idn_at = 16u;
        osal_uint32_t at_bits = 0u;

        // generate at mapping over all at's of specified slave
        // at mapping is stored at idn 16 and should be written in preop
        // state by user
        for (atn = 0u; atn < slv->eeprom.general.soe_channels; ++atn) {
            ec_log(100, "SOE_MAPPING", "slave %2d: getting at pd len channel %d\n", 
                    slave, atn);

            osal_uint32_t bits = 0u;
            if (ec_soe_generate_mapping_local(pec, slave, atn, idn_at, &bits) != EC_OK) {
                continue;
            }

            at_bits += bits;
            bits /= 8u;

            // we only care about whole bytes
            slv->subdevs[atn].pdin.len = bits;
        }

        if (at_bits != 0u) {
            const osal_uint32_t at_sm = 3u;

            ec_log(10, "SOE_MAPPING", "slave %2d: sm%d length bits %d, bytes %d\n", 
                    slave, at_sm, at_bits, (at_bits + 7u) / 8u);

            if (slv->sm_ch > at_sm) {
                slv->sm[at_sm].len = (at_bits + 7u) / 8u;
            }
        }

        const osal_uint16_t idn_mdt = 24u;
        osal_uint32_t mdt_bits = 0u;

        // generate mdt mapping over all mdt's of specified slave
        // mdt mapping is stored at idn 24 and should be written in preop
        // state by user
        for (atn = 0u; atn < slv->eeprom.general.soe_channels; ++atn) {
            ec_log(100, "SOE_MAPPING", "slave %2d: getting mdt pd len channel %d\n", 
                    slave, atn);

            osal_uint32_t bits = 0u;
            if (ec_soe_generate_mapping_local(pec, slave, atn, idn_mdt, &bits) != EC_OK) {
                continue;
            }

            mdt_bits += bits;
            bits /= 8u;

            // we only care about whole bytes
            slv->subdevs[atn].pdout.len = bits;
        }

        if (mdt_bits != 0u) {
            const osal_uint32_t mdt_sm = 2u;
            ec_log(10, "SOE_MAPPING", "slave %2d: sm%d length bits %d, bytes %d\n", 
                    slave, mdt_sm, mdt_bits, (mdt_bits + 7u) / 8u);

            if (slv->sm_ch > mdt_sm) {
                slv->sm[mdt_sm].len = (mdt_bits + 7u) / 8u;
            }
        }
    }

    return ret;
}

