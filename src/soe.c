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

#include "libethercat/slave.h"
#include "libethercat/ec.h"
#include "libethercat/soe.h"
#include "libethercat/timer.h"

#include <stdio.h>
#include <string.h>

//! SoE mailbox structure
typedef struct PACKED ec_soe_header {
    uint8_t op_code    : 3;         //!< op code
    uint8_t incomplete : 1;         //!< incompletion flag
    uint8_t error      : 1;         //!< error flag
    uint8_t atn        : 3;         //!< at number
    uint8_t elements;               //!< servodrive element mask
    union {
        uint16_t idn;               //!< id number
        uint16_t fragments_left;    //!< fragments left
    };
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

//! initialize SoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_soe_init(ec_t *pec, uint16_t slave) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    pool_open(&slv->mbx.soe.recv_pool, 0, 1518);
}

//! deinitialize SoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_soe_deinit(ec_t *pec, uint16_t slave) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    pool_close(slv->mbx.soe.recv_pool);
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
void ec_soe_wait(ec_t *pec, uint16_t slave, pool_entry_t **pp_entry) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    ec_mbx_sched_read(pec, slave);

    ec_timer_t timeout;
    ec_timer_init(&timeout, EC_DEFAULT_TIMEOUT_MBX);

    pool_get(slv->mbx.soe.recv_pool, pp_entry, &timeout);
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
void ec_soe_enqueue(ec_t *pec, uint16_t slave, pool_entry_t *p_entry) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    pool_put(slv->mbx.soe.recv_pool, p_entry);
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
 * \param[in,out] buf   Buffer for where to store the answer. This can either be
 *                      NULL with 'len' also set to zero, or a pointer to a 
 *                      preallocated buffer with set \p len field. In case \p buf is NULL
 *                      it will be allocated by \link ec_soe_read \endlink call. Then 
 *                      you have to make sure, that the buffer is freed by your application.
 * \param[in,out] len   Length of \p buf, see 'buf' descriptions. Returns length of answer.
 * \return 0 on successs
 */
int ec_soe_read(ec_t *pec, uint16_t slave, uint8_t atn, uint16_t idn, 
        uint8_t elements, uint8_t *buf, size_t *len) 
{
    int wkc = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    pthread_mutex_lock(&slv->mbx_lock);

    pool_entry_t *p_entry;
    pool_get(slv->mbx.message_pool_free, &p_entry, NULL);
    memset(p_entry->data, 0, p_entry->data_size);
    MESSAGE_POOL_DEBUG(free);

    ec_soe_request_t *write_buf = (ec_soe_request_t *)(p_entry->data);

    // mailbox header
    write_buf->mbx_hdr.length   = sizeof(ec_soe_header_t);
    write_buf->mbx_hdr.mbxtype  = EC_MBX_SOE;
    // soe header
    write_buf->soe_hdr.op_code    = EC_SOE_READ_REQ;
    write_buf->soe_hdr.atn        = atn;
    write_buf->soe_hdr.elements   = elements;
    write_buf->soe_hdr.idn        = idn;

    // send request
    ec_mbx_enqueue(pec, slave, p_entry);

    uint8_t *to = buf;
    ssize_t left_len = *len;

    while (1) {
        // wait for answer
        for (p_entry = NULL; !p_entry; ec_soe_wait(pec, slave, &p_entry)) {}
        ec_soe_request_t *read_buf  = (ec_soe_request_t *)(p_entry->data); 
    
        // check for correct op_code
        if (read_buf->soe_hdr.op_code != EC_SOE_READ_RES) {
            ec_log(10, __func__, "got unexpected response %d\n", read_buf->soe_hdr.op_code);
            pool_put(slv->mbx.message_pool_free, p_entry);
            continue; // TODO handle unexpected answer
        }

        if (left_len > 0) {
            size_t read_len = read_buf->mbx_hdr.length - sizeof(ec_soe_header_t);
            memcpy(to, &read_buf->data, min(read_len, left_len));
            to += read_len;
            left_len -= read_len;
        }
    
        pool_put(slv->mbx.message_pool_free, p_entry);

        if (/*(left_len < 0) ||*/ !read_buf->soe_hdr.incomplete)
            break;
    }
    
    pthread_mutex_unlock(&slv->mbx_lock);
    
    return wkc;
}

int ec_soe_write(ec_t *pec, uint16_t slave, uint8_t atn, uint16_t idn, 
        uint8_t elements, uint8_t *buf, size_t len) {
    int wkc = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    pthread_mutex_lock(&slv->mbx_lock);

    pool_entry_t *p_entry;
    pool_get(slv->mbx.message_pool_free, &p_entry, NULL);
    memset(p_entry->data, 0, p_entry->data_size);
    MESSAGE_POOL_DEBUG(free);

    ec_soe_request_t *write_buf = (ec_soe_request_t *)(p_entry->data);

    // mailbox header
    write_buf->mbx_hdr.length   = sizeof(ec_soe_header_t);
    write_buf->mbx_hdr.mbxtype  = EC_MBX_SOE;
    // soe header
    write_buf->soe_hdr.op_code    = EC_SOE_WRITE_REQ;
    write_buf->soe_hdr.atn        = atn;
    write_buf->soe_hdr.elements   = elements;
    write_buf->soe_hdr.idn        = idn;

    uint8_t *from = buf;
    size_t left_len = len;
    size_t mbx_len = slv->sm[0].len 
        - sizeof(ec_mbx_header_t) - sizeof(ec_soe_header_t);

    ec_log(100, __func__, "slave %d, atn %d, idn %d, elements %d, buf %p, "
            "len %d, left %d, mbx_len %d\n", 
            slave, atn, idn, elements, buf, len, left_len, mbx_len);

    while (1) {
        size_t send_len = min(left_len, mbx_len);
        write_buf->mbx_hdr.length = sizeof(ec_soe_header_t) + send_len;
        memcpy(&write_buf->data, from, send_len);
        from += send_len;
        left_len -= send_len;

        if (left_len) {
            write_buf->soe_hdr.incomplete = 1;
            write_buf->soe_hdr.fragments_left = left_len / mbx_len + 1;
        } else {
            write_buf->soe_hdr.incomplete = 0;
            write_buf->soe_hdr.idn = idn;
        }

        // send request
        ec_mbx_enqueue(pec, slave, p_entry);

        if (!left_len) {
            // wait for answer
            for (p_entry = NULL; !p_entry; ec_soe_wait(pec, slave, &p_entry)) {}
            ec_soe_request_t *read_buf  = (ec_soe_request_t *)(p_entry->data); 

            // check for correct op_code
            if (read_buf->soe_hdr.op_code != EC_SOE_WRITE_RES) {
                ec_log(10, __func__, "got unexpected response %d\n", read_buf->soe_hdr.op_code);
                pool_put(slv->mbx.message_pool_free, p_entry);
                continue; // TODO handle unexpected answer
            }
        
            pool_put(slv->mbx.message_pool_free, p_entry);
            break;
        }
    }

    pthread_mutex_unlock(&slv->mbx_lock);

    return wkc;
}

static int ec_soe_generate_mapping_local(ec_t *pec, uint16_t slave, uint8_t atn, 
        uint16_t idn, int *bitsize) {
    int ret = 0, i;
    uint16_t *idn_value = NULL;

    *bitsize = 16; // control and status word are always present

    // read size of mapping idn first
    uint16_t idn_len[2];
    size_t idn_len_size = sizeof(idn_len);
    if (ec_soe_read(pec, slave, atn, idn, EC_SOE_VALUE, 
                (uint8_t *)idn_len, &idn_len_size) != 1) {
        ret = -1;
        goto exit;
    }

    // read mapping idn
    size_t idn_size = (idn_len[0] + 4);// / 2;
    idn_value = malloc(idn_size);
    if (ec_soe_read(pec, slave, atn, idn, EC_SOE_VALUE, 
                (uint8_t *)idn_value, &idn_size) != 1) {
        ret = -1;
        goto exit;
    }

    // read all mapped idn's and add bit length, 
    // length is stored in idn attributes
    for (i = 0; i < (idn_len[0]/2); ++i) {
        uint16_t sub_idn = idn_value[i+2];
        ec_soe_idn_attribute_t sub_idn_attr;
        size_t sub_idn_attr_size = sizeof(sub_idn_attr);

        ec_log(100, __func__, "atn %d, read mapped idn %d\n", atn, sub_idn);

        if (ec_soe_read(pec, slave, atn, sub_idn, EC_SOE_ATTRIBUTE, 
                    (uint8_t*)&sub_idn_attr, &sub_idn_attr_size) != 1)
            continue;

        // 0 = 8 bit, 1 = 16 bit, ...
        *bitsize += 8 << sub_idn_attr.length;
    }

exit:
    if (idn_value)
        free(idn_value);

    ec_log(10, __func__, "soe mapping for idn %d, bitsize %d\n", idn, *bitsize);
    return ret;
}

int ec_soe_generate_mapping(ec_t *pec, uint16_t slave) {
    int atn;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    uint16_t idn_at = 16;
    int at_bits = 0, at_sm = 3;
    // generate at mapping over all at's of specified slave
    // at mapping is stored at idn 16 and should be written in preop
    // state by user
    for (atn = 0; atn < slv->eeprom.general.soe_channels; ++atn) {
        ec_log(100, __func__, "slave %2d: getting at pd len channel %d\n", 
                slave, atn);

        int bits = 0;
        if (ec_soe_generate_mapping_local(pec, slave, atn, 
                    idn_at, &bits) != 0)
            continue;

        at_bits += bits;

        // we only care about whole bytes
        slv->subdevs[atn].pdin.len = bits/8;
    }

    if (at_bits) {
        ec_log(10, __func__, "slave %2d: sm%d length bits %d, bytes %d\n", 
                slave, at_sm, at_bits, (at_bits + 7) / 8);

        if (slv->sm && slv->sm_ch > at_sm)
            slv->sm[at_sm].len = (at_bits + 7) / 8;
    }

    uint16_t idn_mdt = 24;
    int mdt_bits = 0, mdt_sm = 2;
    // generate mdt mapping over all mdt's of specified slave
    // mdt mapping is stored at idn 24 and should be written in preop
    // state by user
    for (atn = 0; atn < slv->eeprom.general.soe_channels; ++atn) {
        ec_log(100, __func__, "slave %2d: getting mdt pd len channel %d\n", 
                slave, atn);
    
        int bits = 0;
        if (ec_soe_generate_mapping_local(pec, slave, atn, 
                    idn_mdt, &bits) != 0)
            continue;

        mdt_bits += bits;
        
        // we only care about whole bytes
        slv->subdevs[atn].pdout.len = bits/8;
    }

    if (mdt_bits) {
        ec_log(10, __func__, "slave %2d: sm%d length bits %d, bytes %d\n", 
                slave, mdt_sm, mdt_bits, (mdt_bits + 7) / 8);

        if (slv->sm && slv->sm_ch > mdt_sm)
            slv->sm[mdt_sm].len = (mdt_bits + 7) / 8;
    }

    return -1;
}

