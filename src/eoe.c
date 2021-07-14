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

//! initialize EoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_eoe_init(ec_t *pec, uint16_t slave) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    pool_open(&slv->mbx.eoe.recv_pool, 0, 1518);
}

//! deinitialize EoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_eoe_deinit(ec_t *pec, uint16_t slave) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    pool_close(slv->mbx.eoe.recv_pool);
}

//! \brief Wait for EoE message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] pp_entry  Returns pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_eoe_wait(ec_t *pec, uint16_t slave, pool_entry_t **pp_entry) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    ec_mbx_sched_read(pec, slave);

    ec_timer_t timeout;
    ec_timer_init(&timeout, EC_DEFAULT_TIMEOUT_MBX);

    pool_get(slv->mbx.eoe.recv_pool, pp_entry, &timeout);
}

//! \brief Enqueue EoE message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] slave     Number of ethercat slave. this depends on 
 *                      the physical order of the ethercat slaves 
 *                      (usually the n'th slave attached).
 * \param[in] p_entry   Pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_eoe_enqueue(ec_t *pec, uint16_t slave, pool_entry_t *p_entry) {
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];
    pool_put(slv->mbx.eoe.recv_pool, p_entry);
}

// read coe sdo 
int ec_eoe_set_ip_parameter(ec_t *pec, uint16_t slave, uint8_t *mac,
        uint8_t *ip_address, uint8_t *subnet, uint8_t *gateway, 
        uint8_t *dns, char *dns_name) {
    int ret = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    pthread_mutex_lock(&slv->mbx.lock);

    pool_entry_t *p_entry;
    pool_get(slv->mbx.message_pool_free, &p_entry, NULL);
    memset(p_entry->data, 0, p_entry->data_size);

    ec_eoe_set_ip_parameter_request_t *write_buf = (ec_eoe_set_ip_parameter_request_t *)(p_entry->data);
    
    // mailbox header
    // (mbxhdr (6) - mbxhdr.length (2)) + eoehdr (4) + siphdr (4)
    write_buf->mbx_hdr.length    = 8;
    write_buf->mbx_hdr.mbxtype   = EC_MBX_EOE;
    // eoe header
    write_buf->eoe_hdr.frame_type         = EOE_FRAME_TYPE_SET_ADDRESS_FILTER_REQUEST;
    write_buf->eoe_hdr.last_fragment      = 0x01;

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
    ec_mbx_enqueue(pec, slave, p_entry);

    // wait for answer
    for (p_entry = NULL; !p_entry; ec_eoe_wait(pec, slave, &p_entry)) {}
    ec_eoe_set_ip_parameter_response_t *read_buf = (ec_eoe_set_ip_parameter_response_t *)(p_entry->data);

    ret = read_buf->sip_hdr.result;

    pool_put(slv->mbx.message_pool_free, p_entry);

    pthread_mutex_unlock(&slv->mbx.lock);

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
    int ret = 0;
    ec_slave_t *slv = (ec_slave_t *)&pec->slaves[slave];

    size_t mbx_len = slv->sm[MAILBOX_WRITE].len;
    size_t frag_len = (mbx_len - sizeof(ec_mbx_header_t) - sizeof(ec_eoe_header_t));

    pthread_mutex_lock(&slv->mbx.lock);

    pool_entry_t *p_entry;
    
    size_t rest_len = frame_len;
    int fragment_number = 0;

    while (rest_len > 0) {
        pool_get(slv->mbx.message_pool_free, &p_entry, NULL);
        memset(p_entry->data, 0, p_entry->data_size);

        ec_eoe_request_t *write_buf = (ec_eoe_request_t *)(p_entry->data);

        // mailbox header
        // (mbxhdr (6) - mbxhdr.length (2)) + eoehdr (8) + sdohdr (4)
        write_buf->mbx_hdr.mbxtype   = EC_MBX_EOE;

        // eoe header
        write_buf->eoe_hdr.frame_type         = EOE_FRAME_TYPE_REQUEST;
        write_buf->eoe_hdr.last_fragment      = 0x01;
        write_buf->eoe_hdr.complete_size      = (frame_len + 31)/32;
        write_buf->eoe_hdr.fragment_number    = fragment_number++;

        if (rest_len > frag_len) {
            write_buf->eoe_hdr.last_fragment = 0x00;
        } else {
            write_buf->eoe_hdr.last_fragment = 0x01;
        }
    
        size_t act_len = min(frag_len, rest_len);
        write_buf->mbx_hdr.length = 4 + act_len;
        memcpy(&write_buf->data.bdata[0], &frame[frame_len - rest_len], act_len);

        // send request
        ec_mbx_enqueue(pec, slave, p_entry);

        rest_len -= frag_len;
    }

    pthread_mutex_unlock(&slv->mbx.lock);

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
    int ret = 0;
#if 0
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

    pthread_mutex_unlock(&slv->mbx.lock);
#endif
    return ret;
}

