//! ethercat ethernet over ethercat mailbox handling
/*!
 * author: Robert Burger <robert.burger@dlr.de>
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

#include "libethercat/ec.h"
#include "libethercat/eoe.h"
#include "libethercat/timer.h"
#include "libethercat/error_codes.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    // 8 bit
    unsigned frame_type         : 4;
    unsigned port               : 4;    

    // 8 bit
    unsigned last_fragment      : 1;
    unsigned time_appended      : 1;
    unsigned time_requested     : 1;
    unsigned reserved           : 5;

    // 16 bit
    unsigned fragment_number    : 6;
    unsigned complete_size      : 6;
    unsigned frame_number       : 4;
} PACKED ec_eoe_header_t;

const static int EOE_FRAME_TYPE_REQUEST                         = 0;
const static int EOE_FRAME_TYPE_RESPONSE                        = 3;
const static int EOE_FRAME_TYPE_FRAGMENT_DATA                   = 0;
const static int EOE_FRAME_TYPE_SET_IP_ADDRESS_REQUEST          = 2;
const static int EOE_FRAME_TYPE_SET_IP_ADDRESS_RESPONSE         = 3;
const static int EOE_FRAME_TYPE_SET_ADDRESS_FILTER_REQUEST      = 4;
const static int EOE_FRAME_TYPE_SET_ADDRESS_FILTER_RESPONSE     = 5;

// ------------------------ EoE REQUEST --------------------------

//! initiate eoe request
typedef struct {
    ec_mbx_header_t mbx_hdr;
    ec_eoe_header_t eoe_hdr;
    ec_data_t       data;
} PACKED ec_eoe_request_t;

//! eoe response
typedef struct {
    ec_mbx_header_t mbx_hdr;
    ec_eoe_header_t eoe_hdr;
    uint32_t        time_stamp;
} PACKED ec_eoe_response_t;

// ------------------------ EoE SET IP ADDRESS REQUEST --------------------------

typedef struct {
    unsigned mac_included           : 1;
    unsigned ip_address_included    : 1;
    unsigned subnet_included        : 1;
    unsigned gateway_included       : 1;
    unsigned dns_included           : 1;
    unsigned dns_name_included      : 1;
    unsigned reserved               : 26;
} PACKED ec_eoe_set_ip_parameter_header_t;

//! initiate eoe set ip parameter request
typedef struct {
    ec_mbx_header_t                     mbx_hdr;
    ec_eoe_header_t                     eoe_hdr;
    ec_eoe_set_ip_parameter_header_t    sip_hdr;
    ec_data_t                           data;
} PACKED ec_eoe_set_ip_parameter_request_t;


typedef struct {
    // 8 bit
    unsigned frame_type         : 4;
    unsigned port               : 4;    

    // 8 bit
    unsigned last_fragment      : 1;
    unsigned time_appended      : 1;
    unsigned time_requested     : 1;
    unsigned reserved           : 5;

    // 16 bit
    unsigned result             : 16;
} PACKED ec_eoe_set_ip_parameter_response_header_t;

typedef struct {
    ec_mbx_header_t                             mbx_hdr;
    ec_eoe_set_ip_parameter_response_header_t   sip_hdr;
} PACKED ec_eoe_set_ip_parameter_response_t;

// read coe sdo 
int ec_eoe_set_ip_parameter(ec_t *pec, uint16_t slave, uint8_t *mac,
        uint8_t *ip_address, uint8_t *subnet, uint8_t *gateway, 
        uint8_t *dns, char *dns_name) {
    int wkc, ret = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    if (!(slv->eeprom.mbx_supported & EC_EEPROM_MBX_EOE))
        return EC_ERROR_MAILBOX_NOT_SUPPORTED_EOE;
    if (!(slv->mbx_write.buf))
        return EC_ERROR_MAILBOX_WRITE_IS_NULL;
    if (!(slv->mbx_read.buf))
        return EC_ERROR_MAILBOX_READ_IS_NULL;

    pthread_mutex_lock(&slv->mbx_lock);

    ec_eoe_set_ip_parameter_request_t *write_buf = 
        (ec_eoe_set_ip_parameter_request_t *)(slv->mbx_write.buf);
    
    // empty mailbox if anything in
    ec_mbx_clear(pec, slave, 1);
    ec_mbx_receive(pec, slave, 0);

    // mailbox header
    // (mbxhdr (6) - mbxhdr.length (2)) + eoehdr (4) + siphdr (4)
    ec_mbx_clear(pec, slave, 0);
    write_buf->mbx_hdr.length    = 8;

    write_buf->mbx_hdr.address   = 0x0000;
    write_buf->mbx_hdr.priority  = 0x00;
    write_buf->mbx_hdr.mbxtype   = EC_MBX_EOE;

    // eoe header
    write_buf->eoe_hdr.frame_type         = EOE_FRAME_TYPE_SET_ADDRESS_FILTER_REQUEST;
    write_buf->eoe_hdr.port               = 0x00;    
    write_buf->eoe_hdr.last_fragment      = 0x01;
    write_buf->eoe_hdr.time_appended      = 0x00;
    write_buf->eoe_hdr.time_requested     = 0x00;
    write_buf->eoe_hdr.reserved           = 0x00;
    write_buf->eoe_hdr.fragment_number    = 0x00;
    write_buf->eoe_hdr.complete_size      = 0x00;
    write_buf->eoe_hdr.frame_number       = 0x00;

    memset(&(write_buf->sip_hdr), 0, sizeof(write_buf->sip_hdr));
    uint8_t *buf = &write_buf->data.bdata[0];

#define EOE_SIZEOF_MAC          6
#define EOE_SIZEOF_IP_ADDRESS   4
#define EOE_SIZEOF_SUBNET       4
#define EOE_SIZEOF_GATEWAY      4
#define EOE_SIZEOF_DNS          4
#define EOE_SIZEOF_DNS_NAME     32

#define ADD_PARAMETER(parameter, size)                  \
    if ((parameter)) {                                  \
        memcpy(buf, (parameter), (size));               \
        buf += (size);                                  \
        write_buf->sip_hdr.parameter ## _included = 1;  \
        write_buf->mbx_hdr.length += size; }            \

    ADD_PARAMETER(mac,          EOE_SIZEOF_MAC);
    ADD_PARAMETER(ip_address,   EOE_SIZEOF_IP_ADDRESS);
    ADD_PARAMETER(subnet,       EOE_SIZEOF_SUBNET);
    ADD_PARAMETER(gateway,      EOE_SIZEOF_GATEWAY);
    ADD_PARAMETER(dns,          EOE_SIZEOF_DNS);
    ADD_PARAMETER(dns_name,     EOE_SIZEOF_DNS_NAME);
    
    // send request
    wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
    if (!wkc) {
        ec_log(10, "ec_eoe_set_ip_parameter", "error on writing send mailbox\n");
        ret = EC_ERROR_MAILBOX_WRITE;
        goto exit;
    }

    // wait for answer
    ec_mbx_clear(pec, slave, 1);
    wkc = ec_mbx_receive(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
    if (!wkc) {
        ec_log(10, "ec_eoe_set_ip_parameter", "error on reading receive mailbox\n");
        ret = EC_ERROR_MAILBOX_READ;
        goto exit;
    }

    ec_eoe_set_ip_parameter_response_t *read_buf  = 
        (ec_eoe_set_ip_parameter_response_t *)(slv->mbx_read.buf); 

    ret = read_buf->sip_hdr.result;

exit:
    // reset mailbox state 
    if (slv->mbx_read.sm_state) {
        *slv->mbx_read.sm_state = 0;
        slv->mbx_read.skip_next = 1;
    }

    pthread_mutex_unlock(&slv->mbx_lock);

    return ret;
}

// send ethernet frame
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[in] frame         Ethernet frame buffer to be sent.
 * \param[in] frame_len     Length of Ethernet frame buffer.
 *
 * \return 0 on success, otherwise error code.
 */
int ec_eoe_send_frame(ec_t *pec, uint16_t slave, uint8_t *frame, 
        size_t frame_len) {
    int wkc, ret = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    if (!(slv->eeprom.mbx_supported & EC_EEPROM_MBX_EOE))
        return EC_ERROR_MAILBOX_NOT_SUPPORTED_EOE;
    if (!(slv->mbx_write.buf))
        return EC_ERROR_MAILBOX_WRITE_IS_NULL;
    if (!(slv->mbx_read.buf))
        return EC_ERROR_MAILBOX_READ_IS_NULL;

    size_t mbx_len = slv->sm[slv->mbx_write.sm_nr].len;
    size_t frag_len = (mbx_len - sizeof(ec_mbx_header_t) - sizeof(ec_eoe_header_t));

    pthread_mutex_lock(&slv->mbx_lock);

    ec_eoe_request_t *write_buf = 
        (ec_eoe_request_t *)(slv->mbx_write.buf);
    
    // empty mailbox if anything in
    ec_mbx_clear(pec, slave, 1);
    ec_mbx_receive(pec, slave, 0);

    // mailbox header
    // (mbxhdr (6) - mbxhdr.length (2)) + eoehdr (8) + sdohdr (4)
    ec_mbx_clear(pec, slave, 0);

    write_buf->mbx_hdr.address   = 0x0000;
    write_buf->mbx_hdr.priority  = 0x00;
    write_buf->mbx_hdr.mbxtype   = EC_MBX_EOE;

    // eoe header
    write_buf->eoe_hdr.frame_type         = EOE_FRAME_TYPE_REQUEST;
    write_buf->eoe_hdr.port               = 0x00;    
    write_buf->eoe_hdr.last_fragment      = 0x01;
    write_buf->eoe_hdr.time_appended      = 0x00;
    write_buf->eoe_hdr.time_requested     = 0x00;
    write_buf->eoe_hdr.reserved           = 0x00;
    write_buf->eoe_hdr.fragment_number    = 0x00;
    write_buf->eoe_hdr.complete_size      = (frame_len + 31)/32;
    write_buf->eoe_hdr.frame_number       = 0x00;

    size_t rest_len = frame_len;

    while (rest_len > 0) {
        write_buf->eoe_hdr.fragment_number++;

        if (rest_len > frag_len) {
            write_buf->eoe_hdr.last_fragment = 0x00;
        } else {
            write_buf->eoe_hdr.last_fragment = 0x01;
        }
    
        size_t act_len = min(frag_len, rest_len);
        write_buf->mbx_hdr.length = 4 + act_len;
        memcpy(&write_buf->data.bdata[0], &frame[frame_len - rest_len], act_len);

        // send request
        wkc = ec_mbx_send(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
        if (!wkc) {
            ec_log(10, "ec_eoe_send_frame", "error on writing send mailbox\n");
            ret = EC_ERROR_MAILBOX_WRITE;
            goto exit;
        }

        rest_len -= frag_len;
    }

exit:
    // reset mailbox state 
    if (slv->mbx_read.sm_state) {
        *slv->mbx_read.sm_state = 0;
        slv->mbx_read.skip_next = 1;
    }

    pthread_mutex_unlock(&slv->mbx_lock);

    return ret;
}

// receive ethernet frame
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 * \param[in] frame         Ethernet frame buffer to be sent.
 * \param[in] frame_len     Length of Ethernet frame buffer.
 *
 * \return 0 on success, otherwise error code.
 */
int ec_eoe_recv_frame(ec_t *pec, uint16_t slave, uint8_t *frame, 
        size_t frame_len) {
    int wkc, ret = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    
    // wait for answer
    ec_mbx_clear(pec, slave, 1);
    wkc = ec_mbx_receive(pec, slave, EC_DEFAULT_TIMEOUT_MBX);
    if (!wkc) {
        ec_log(10, "ec_eoe_set_ip_parameter", "error on reading receive mailbox\n");
        ret = EC_ERROR_MAILBOX_READ;
        goto exit;
    }

//    ec_eoe_set_ip_parameter_response_t *read_buf  = 
//        (ec_eoe_set_ip_parameter_response_t *)(slv->mbx_read.buf); 

exit:
    // reset mailbox state 
    if (slv->mbx_read.sm_state) {
        *slv->mbx_read.sm_state = 0;
        slv->mbx_read.skip_next = 1;
    }

    pthread_mutex_unlock(&slv->mbx_lock);

    return ret;
}

