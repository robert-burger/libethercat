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

#include "libethercat/mbx.h"
#include "libethercat/soe.h"
#include "libethercat/timer.h"

#include <stdio.h>
#include <string.h>

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


//! read elements of soe id number
/*!
 * \param pec pointer to ethercat master
 * \param slave ethercat slave number
 * \param atn at number
 * \param idn id number
 * \param element servodrive elements
 * \param buf buffer for answer
 * \param len length of buffer, returns length of answer
 * \return 0 on successs
 */
int ec_soe_read(ec_t *pec, uint16_t slave, uint8_t atn, uint16_t idn, 
        uint8_t elements, uint8_t *buf, size_t *len) {
    int wkc = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    if (!(slv->eeprom.mbx_supported & EC_EEPROM_MBX_SOE))
        return 0;

    pthread_mutex_lock(&slv->mbx_lock);

    ec_soe_request_t *write_buf = 
        (ec_soe_request_t *)(slv->mbx_write.buf);

    // empty mailbox if anything in
    ec_mbx_clear(pec, slave, 1);
    ec_mbx_receive(pec, slave, 0);

    // mailbox header
    ec_mbx_clear(pec, slave, 0);
    write_buf->mbx_hdr.length   = sizeof(ec_soe_header_t);
    write_buf->mbx_hdr.address  = 0x0000;
    write_buf->mbx_hdr.priority = 0x00;
    write_buf->mbx_hdr.mbxtype  = EC_MBX_SOE;
    write_buf->mbx_hdr.counter  = 0;

    // soe header
    write_buf->soe_hdr.op_code    = EC_SOE_READ_REQ;
    write_buf->soe_hdr.incomplete = 0;
    write_buf->soe_hdr.error      = 0;
    write_buf->soe_hdr.atn        = atn;
    write_buf->soe_hdr.elements   = elements;
    write_buf->soe_hdr.idn        = idn;

    // send request
    if (!(wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX))) {
        ec_log(10, __func__, "error on writing send mailbox\n");
        goto exit;
    }

    uint8_t *to = buf;
    ssize_t left_len = *len;
    ec_soe_request_t *read_buf  = 
        (ec_soe_request_t *)(slv->mbx_read.buf); 

    while (1) {
        // wait for answer
        ec_mbx_clear(pec, slave, 1);
        if (!(wkc = ec_mbx_receive(pec, slave, EC_DEFAULT_TIMEOUT_MBX))) {
            ec_log(10, __func__, "error on reading receive mailbox\n");
            goto exit;
        }

        // check for correct op_code
        if (read_buf->soe_hdr.op_code != EC_SOE_READ_RES) {
            ec_log(10, __func__, "got unexpected response %d\n", 
                    read_buf->soe_hdr.op_code);
            continue; // TODO handle unexpected answer
        }

        if (left_len > 0) {
            size_t read_len = read_buf->mbx_hdr.length - 
                sizeof(ec_soe_header_t);
            memcpy(to, &read_buf->data, min(read_len, left_len));
            to += read_len;
            left_len -= read_len;
        }

        if (/*(left_len < 0) ||*/ !read_buf->soe_hdr.incomplete)
            break;
    }
    
exit:
    // reset mailbox state 
    if (slv->mbx_read.sm_state)
        *slv->mbx_read.sm_state = 0;

    pthread_mutex_unlock(&slv->mbx_lock);
    
    return wkc;
}

//! write elements of soe id number
/*!
 * \param pec pointer to ethercat master
 * \param slave ethercat slave number
 * \param atn at number
 * \param idn id number
 * \param element servodrive elements
 * \param buf buffer to write
 * \param len length of buffer
 * \return 0 on successs
 */
int ec_soe_write(ec_t *pec, uint16_t slave, uint8_t atn, uint16_t idn, 
        uint8_t elements, uint8_t *buf, size_t len) {
    int wkc = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    if (!(slv->eeprom.mbx_supported & EC_EEPROM_MBX_SOE))
        return 0;

    pthread_mutex_lock(&slv->mbx_lock);

    ec_soe_request_t *write_buf = 
        (ec_soe_request_t *)(slv->mbx_write.buf);

    // empty mailbox if anything in
    ec_mbx_clear(pec, slave, 1);
    while (ec_mbx_receive(pec, slave, EC_SHORT_TIMEOUT_MBX) != 0)
        ;

    // mailbox header
    ec_mbx_clear(pec, slave, 0);
    write_buf->mbx_hdr.length   = sizeof(ec_soe_header_t);
    write_buf->mbx_hdr.address  = 0x0000;
    write_buf->mbx_hdr.priority = 0x00;
    write_buf->mbx_hdr.mbxtype  = EC_MBX_SOE;

    // soe header
    write_buf->soe_hdr.op_code    = EC_SOE_WRITE_REQ;
    write_buf->soe_hdr.error      = 0;
    write_buf->soe_hdr.atn        = atn;
    write_buf->soe_hdr.elements   = elements;
    write_buf->soe_hdr.idn        = idn;

    uint8_t *from = buf;
    size_t left_len = len;
    size_t mbx_len = slv->sm[0].len 
        - sizeof(ec_mbx_header_t) - sizeof(ec_soe_header_t);
    ec_soe_request_t *read_buf  = 
        (ec_soe_request_t *)(slv->mbx_read.buf); 

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
        if (!(wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX))) {
            ec_log(10, __func__, "error on writing send mailbox\n");
            goto exit;
        }

        if (!left_len) {
            // wait for answer
            ec_mbx_clear(pec, slave, 1);
            if (!(wkc = ec_mbx_receive(pec, slave, i
                            EC_DEFAULT_TIMEOUT_MBX * 10))) {
                ec_log(10, __func__, "error on reading receive mailbox\n");
                goto exit;
            }

            // check for correct op_code
            if (read_buf->soe_hdr.op_code != EC_SOE_WRITE_RES) {
                ec_log(10, __func__, "got unexpected response %d\n", 
                        read_buf->soe_hdr.op_code);
                continue; // TODO handle unexpected answer
            }

            break;
        }
    }

exit:
    // reset mailbox state 
    if (slv->mbx_read.sm_state)
        *slv->mbx_read.sm_state = 0;

    pthread_mutex_unlock(&slv->mbx_lock);

    return wkc;
}

int ec_soe_generate_mapping_local(ec_t *pec, uint16_t slave, uint8_t atn, 
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

//! generate sync manager process data mapping via soe
/*!
 * \param pec pointer to ethercat master
 * \param slave slave number
 * \return 0 on success
 */
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

