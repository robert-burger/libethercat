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
 * If not, see <www.gnu.org/licenses/>.
 */

#include <libethercat/config.h>

#include "libethercat/ec.h"
#include "libethercat/eoe.h"
#include "libethercat/error_codes.h"

#include <assert.h>
// cppcheck-suppress misra-c2012-21.6
#include <stdio.h>
#include <string.h>

#if LIBETHERCAT_HAVE_UNISTD_H == 1
#include <unistd.h>
#endif

#if LIBETHERCAT_HAVE_FCNTL_H == 1
#include <fcntl.h>
#endif

#if LIBETHERCAT_HAVE_SYS_TYPES_H == 1
#include <sys/types.h>
#endif

#if LIBETHERCAT_HAVE_SYS_STAT_H == 1
#include <sys/stat.h>
#endif

#if LIBETHERCAT_HAVE_SYS_SOCKET_H == 1
#include <sys/socket.h>
#endif

#if LIBETHERCAT_HAVE_SYS_IOCTL_H == 1
#include <sys/ioctl.h>
#endif

#if LIBETHERCAT_HAVE_INTTYPES_H == 1
#include <inttypes.h>
#endif

#if LIBETHERCAT_BUILD_POSIX
#include <linux/if.h>
#include <linux/if_tun.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <errno.h>

// forward declarations
static int ec_eoe_process_recv(ec_t *pec, osal_uint16_t slave);

typedef struct {
    // 8 bit
    osal_uint8_t frame_type         : 4;
    osal_uint8_t port               : 4;    

    // 8 bit
    osal_uint8_t last_fragment      : 1;
    osal_uint8_t time_appended      : 1;
    osal_uint8_t time_requested     : 1;
    osal_uint8_t reserved           : 5;

    // 16 bit
    osal_uint16_t fragment_number    : 6;
    osal_uint16_t complete_size      : 6;
    osal_uint16_t frame_number       : 4;
} PACKED ec_eoe_header_t;

#define EOE_FRAME_TYPE_REQUEST                         0u
#define EOE_FRAME_TYPE_RESPONSE                        3u
#define EOE_FRAME_TYPE_FRAGMENT_DATA                   0u
#define EOE_FRAME_TYPE_SET_IP_ADDRESS_REQUEST          2u
#define EOE_FRAME_TYPE_SET_IP_ADDRESS_RESPONSE         3u
#define EOE_FRAME_TYPE_SET_ADDRESS_FILTER_REQUEST      4u
#define EOE_FRAME_TYPE_SET_ADDRESS_FILTER_RESPONSE     5u

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
    osal_uint32_t        time_stamp; // cppcheck-suppress unusedStructMember
} PACKED ec_eoe_response_t;

// ------------------------ EoE SET IP ADDRESS REQUEST --------------------------

typedef struct {
    osal_uint32_t mac_included           : 1;
    osal_uint32_t ip_address_included    : 1;
    osal_uint32_t subnet_included        : 1;
    osal_uint32_t gateway_included       : 1;
    osal_uint32_t dns_included           : 1;
    osal_uint32_t dns_name_included      : 1;
    osal_uint32_t reserved               : 26; // cppcheck-suppress unusedStructMember
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
    osal_uint8_t frame_type         : 4; // cppcheck-suppress unusedStructMember
    osal_uint8_t port               : 4; // cppcheck-suppress unusedStructMember

    // 8 bit
    osal_uint8_t last_fragment      : 1; // cppcheck-suppress unusedStructMember
    osal_uint8_t time_appended      : 1; // cppcheck-suppress unusedStructMember
    osal_uint8_t time_requested     : 1; // cppcheck-suppress unusedStructMember
    osal_uint8_t reserved           : 5; // cppcheck-suppress unusedStructMember

    // 16 bit
    osal_uint16_t result             : 16;
} PACKED ec_eoe_set_ip_parameter_response_header_t;

typedef struct {
    ec_mbx_header_t                             mbx_hdr;
    ec_eoe_set_ip_parameter_response_header_t   sip_hdr;
} PACKED ec_eoe_set_ip_parameter_response_t;

typedef struct eth_frame {
    osal_size_t frame_size;
    osal_uint8_t frame_data[1518];
} eth_frame_t;

static void eoe_debug_print(const osal_char_t *ctx, const osal_char_t *msg, osal_uint8_t *frame, osal_size_t frame_len) {
#define EOE_DEBUG_BUFFER_SIZE   1024
    static osal_char_t eoe_debug_buffer[EOE_DEBUG_BUFFER_SIZE];
    
    int pos = snprintf(eoe_debug_buffer, EOE_DEBUG_BUFFER_SIZE, "%s: ", msg);
    for (osal_uint32_t i = 0; (i < frame_len) && (pos < EOE_DEBUG_BUFFER_SIZE); ++i) {
        pos += snprintf(&eoe_debug_buffer[pos], EOE_DEBUG_BUFFER_SIZE-pos, "%02X", frame[i]);
    }

    ec_log(100, ctx, "%s\n", eoe_debug_buffer);
}

//! initialize EoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_eoe_init(ec_t *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

    osal_mutex_init(&slv->mbx.eoe.lock, NULL);

    (void)pool_open(&slv->mbx.eoe.recv_pool, 0, NULL);
    (void)pool_open(&slv->mbx.eoe.response_pool, 0, NULL);
    (void)pool_open(&slv->mbx.eoe.eth_frames_free_pool, 128, &slv->mbx.eoe.free_frames[0]);
    (void)pool_open(&slv->mbx.eoe.eth_frames_recv_pool, 0, NULL);

    osal_semaphore_init(&slv->mbx.eoe.send_sync, 0, 0);
}

//! deinitialize EoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_eoe_deinit(ec_t *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    
    osal_mutex_lock(&slv->mbx.eoe.lock);

    osal_semaphore_destroy(&slv->mbx.eoe.send_sync);
    (void)pool_close(&slv->mbx.eoe.eth_frames_recv_pool);
    (void)pool_close(&slv->mbx.eoe.eth_frames_free_pool);
    (void)pool_close(&slv->mbx.eoe.response_pool);
    (void)pool_close(&slv->mbx.eoe.recv_pool);
    
    osal_mutex_unlock(&slv->mbx.eoe.lock);
    osal_mutex_destroy(&slv->mbx.eoe.lock);
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
static void ec_eoe_wait_response(ec_t *pec, osal_uint16_t slave, pool_entry_t **pp_entry) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(pp_entry != NULL);

    ec_slave_ptr(slv, pec, slave);

    ec_mbx_sched_read(pec, slave);

    osal_timer_t timeout;
    osal_timer_init(&timeout, EC_DEFAULT_TIMEOUT_MBX);

    (void)pool_get(&slv->mbx.eoe.response_pool, pp_entry, &timeout);
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
static void ec_eoe_wait(ec_t *pec, osal_uint16_t slave, pool_entry_t **pp_entry) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(pp_entry != NULL);

    ec_slave_ptr(slv, pec, slave);

    ec_mbx_sched_read(pec, slave);

    osal_timer_t timeout;
    osal_timer_init(&timeout, EC_DEFAULT_TIMEOUT_MBX);

    (void)pool_get(&slv->mbx.eoe.recv_pool, pp_entry, &timeout);
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
void ec_eoe_enqueue(ec_t *pec, osal_uint16_t slave, pool_entry_t *p_entry) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(p_entry != NULL);

    ec_slave_ptr(slv, pec, slave);
    
    // cppcheck-suppress misra-c2012-11.3
    ec_eoe_request_t *write_buf = (ec_eoe_request_t *)(p_entry->data);
    ec_log(100, "EOE_ENQUEUE_FRAGMENT", "slave %2d: got eoe frame type 0x%X\n", slave, 
            write_buf->eoe_hdr.frame_type);
    
    if ((write_buf->eoe_hdr.frame_type == EOE_FRAME_TYPE_SET_IP_ADDRESS_RESPONSE) != 0u) {
        pool_put(&slv->mbx.eoe.response_pool, p_entry);
    } else {
        pool_put(&slv->mbx.eoe.recv_pool, p_entry);

        if (write_buf->eoe_hdr.last_fragment != 0u) {
            ec_log(100, "EOE_ENQUEUE_FRAGMENT", "slave %2d: was last fragment\n", slave);
            
            int ret;

            do {
                ret = ec_eoe_process_recv(pec, slave);
            } while (ret == EC_OK);
        }
    }
}

// read coe sdo 
int ec_eoe_set_ip_parameter(ec_t *pec, osal_uint16_t slave, osal_uint8_t *mac,
        osal_uint8_t *ip_address, osal_uint8_t *subnet, osal_uint8_t *gateway, 
        osal_uint8_t *dns, osal_char_t *dns_name) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    pool_entry_t *p_entry;
    int ret = EC_ERROR_MAILBOX_TIMEOUT;
    ec_slave_ptr(slv, pec, slave);
    
    osal_mutex_lock(&slv->mbx.eoe.lock);
    
    if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_EOE) != EC_OK) {
        ret = EC_ERROR_MAILBOX_NOT_SUPPORTED_EOE;
    } else if (ec_mbx_get_free_send_buffer(pec, slave, &p_entry, NULL) != 0) {
        ret = EC_ERROR_MAILBOX_OUT_OF_SEND_BUFFERS;
    } else {
        ec_log(10, "EOE_SET_IP_PARAMETER", "slave %2d: set ip parameter\n", slave);

        // cppcheck-suppress misra-c2012-11.3
        ec_eoe_set_ip_parameter_request_t *write_buf = (ec_eoe_set_ip_parameter_request_t *)(p_entry->data);

        // mailbox header
        // (mbxhdr (6) - mbxhdr.length (2)) + eoehdr (4) + siphdr (4)
        write_buf->mbx_hdr.length    = 8;
        write_buf->mbx_hdr.mbxtype   = EC_MBX_EOE;
        // eoe header
        write_buf->eoe_hdr.frame_type         = EOE_FRAME_TYPE_SET_IP_ADDRESS_REQUEST;
        write_buf->eoe_hdr.last_fragment      = 0x01;

        osal_off_t data_pos = 0;

#define EOE_SIZEOF_MAC           6u
#define EOE_SIZEOF_IP_ADDRESS    4u
#define EOE_SIZEOF_SUBNET        4u
#define EOE_SIZEOF_GATEWAY       4u
#define EOE_SIZEOF_DNS           4u
#define EOE_SIZEOF_DNS_NAME     32u

        // cppcheck-suppress misra-c2012-20.10
        // cppcheck-suppress misra-c2012-20.12
#define ADD_PARAMETER(parameter, size)                                      \
        if ((parameter) != NULL) {                                          \
            (void)memcpy(&write_buf->data[data_pos], (parameter), (size));  \
            data_pos += (size);                                             \
            write_buf->sip_hdr.parameter ## _included = 1u;                 \
            write_buf->mbx_hdr.length += (size); }

        ADD_PARAMETER(mac,          EOE_SIZEOF_MAC);
        ADD_PARAMETER(ip_address,   EOE_SIZEOF_IP_ADDRESS);
        ADD_PARAMETER(subnet,       EOE_SIZEOF_SUBNET);
        ADD_PARAMETER(gateway,      EOE_SIZEOF_GATEWAY);
        ADD_PARAMETER(dns,          EOE_SIZEOF_DNS);
        ADD_PARAMETER(dns_name,     EOE_SIZEOF_DNS_NAME); // cppcheck-suppress unreadVariable

        // send request and wait for answer
        ec_mbx_enqueue_tail(pec, slave, p_entry);
        ec_eoe_wait_response(pec, slave, &p_entry); 

        if (p_entry != NULL) {
            // cppcheck-suppress misra-c2012-11.3
            ec_eoe_set_ip_parameter_response_t *read_buf = (ec_eoe_set_ip_parameter_response_t *)(p_entry->data);

            ret = read_buf->sip_hdr.result;
            ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
        }
    }

    osal_mutex_unlock(&slv->mbx.eoe.lock);

    return ret;
}
    
static void ec_eoe_send_sync(struct ec *pec, pool_entry_t *p_entry, ec_datagram_t *p_dg) {
    (void)p_dg;

    osal_semaphore_post(&pec->slaves[p_entry->user_arg].mbx.eoe.send_sync);
}

#define ALIGN_32BIT_BLOCKS(a) { (a) = (((a) >> 5) << 5); }

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
int ec_eoe_send_frame(ec_t *pec, osal_uint16_t slave, osal_uint8_t *frame, osal_size_t frame_len) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(frame != NULL);

    int ret = EC_ERROR_MAILBOX_TIMEOUT;
    ec_slave_ptr(slv, pec, slave);
    
    osal_mutex_lock(&slv->mbx.eoe.lock);

    if (ec_mbx_check(pec, slave, EC_EEPROM_MBX_EOE) != EC_OK) {
        ret = EC_ERROR_MAILBOX_NOT_SUPPORTED_EOE;
    } else {
        osal_size_t max_frag_len = (slv->sm[MAILBOX_WRITE].len - sizeof(ec_mbx_header_t) - sizeof(ec_eoe_header_t));
        ALIGN_32BIT_BLOCKS(max_frag_len);
        osal_off_t frame_offset = 0;
        int frag_number = 0;

        do {
            osal_size_t frag_len = frame_len - frame_offset;
            frag_len = min(frag_len, max_frag_len);

            // get mailbox buffer to write frame fragment request
            pool_entry_t *p_entry;
            osal_timer_t timeout;
            (void)osal_timer_gettime(&timeout);
            timeout.sec += 10;
            (void)ec_mbx_get_free_send_buffer(pec, slave, &p_entry, &timeout);

            // send sync callback
            p_entry->user_cb = ec_eoe_send_sync;
            p_entry->user_arg = slv->mbx.slave;

            // cppcheck-suppress misra-c2012-11.3
            ec_eoe_request_t *write_buf = (ec_eoe_request_t *)(p_entry->data);

            ec_log(100, "EOE_SEND_FRAME", "slave %2d: sending eoe fragment %d\n", slave, frag_number);
            // mailbox header
            // (mbxhdr (6) - mbxhdr.length (2)) + eoehdr (8) + sdohdr (4)
            write_buf->mbx_hdr.length           = 4u + frag_len;
            write_buf->mbx_hdr.mbxtype          = EC_MBX_EOE;
            // eoe header
            write_buf->eoe_hdr.frame_type       = EOE_FRAME_TYPE_REQUEST;
            write_buf->eoe_hdr.fragment_number  = frag_number;

            if (frag_len < max_frag_len) { write_buf->eoe_hdr.last_fragment  = 0x01u; } 
            else                         { write_buf->eoe_hdr.last_fragment  = 0x00u; }

            if (frag_number == 0)        { write_buf->eoe_hdr.complete_size  = (frame_len + 31u) >> 5u; }
            else                         { write_buf->eoe_hdr.complete_size  = (frame_offset) >> 5u;   }

            // copy fragment
            (void)memcpy(&write_buf->data[0], &frame[frame_offset], frag_len);
            frame_offset += frag_len;
            frag_number++;

            // send request
            ec_mbx_enqueue_tail(pec, slave, p_entry);
            osal_semaphore_wait(&slv->mbx.eoe.send_sync);
        } while(frame_offset < frame_len);
    }

    osal_mutex_unlock(&slv->mbx.eoe.lock);
    
    return ret;
}

//! \brief Process received ethernet frame.
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
int ec_eoe_process_recv(ec_t *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    int ret = EC_OK;
    ec_slave_ptr(slv, pec, slave);
    pool_entry_t *p_entry = NULL;
    pool_entry_t *p_eth_entry = NULL;

    if (pool_get(&slv->mbx.eoe.recv_pool, &p_entry, NULL) != EC_OK) { // get first received EoE frame, should be start of ethernet frame
        ret = EC_ERROR_UNAVAILABLE; // no error here, but no frame to process
    } else if (pool_get(&slv->mbx.eoe.eth_frames_free_pool, &p_eth_entry, NULL) != EC_OK) {
        // no usable Ethernet frame buffer available, nobody has cared for last received Ethernet frames so far.
        ret = EC_ERROR_OUT_OF_MEMORY;
    } else {
        // cppcheck-suppress misra-c2012-11.3
        eth_frame_t *eth_frame = (eth_frame_t *)(p_eth_entry->data);
        // cppcheck-suppress misra-c2012-11.3
        ec_eoe_request_t *read_buf = (ec_eoe_request_t *)(p_entry->data);

        if (read_buf->eoe_hdr.fragment_number != 0u) {
            ec_log(1, "EOE_RECV", "slave %2d: first EoE fragment is not first fragment (got %d)\n",
                    slave, read_buf->eoe_hdr.fragment_number);

            ret = EC_OK; // signal that we might have further frames to process
        } else {
            eth_frame->frame_size = read_buf->eoe_hdr.complete_size << 5u;
            osal_size_t frag_len       = read_buf->mbx_hdr.length - 4u;
            osal_off_t frame_offset    = 0;

            (void)memcpy(&(eth_frame->frame_data[frame_offset]), &read_buf->data[0], frag_len);
            frame_offset += frag_len;

            if (!read_buf->eoe_hdr.last_fragment) {
                ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
                p_entry = NULL;

                // proceed with next fragment
                ec_eoe_wait(pec, slave, &p_entry); 
                while (p_entry != NULL) {
                    // cppcheck-suppress misra-c2012-11.3
                    read_buf = (ec_eoe_request_t *)(p_entry->data);

                    if (frame_offset != (read_buf->eoe_hdr.complete_size << 5u)) {
                        ec_log(1, "EOE_RECV", "slave %d: frame offset mismatch %" PRIu64 " != %d\n", 
                                slave, frame_offset, read_buf->eoe_hdr.complete_size << 5u);
                    }

                    frag_len = read_buf->mbx_hdr.length - 4u;
                    (void)memcpy(&(eth_frame->frame_data[frame_offset]), &(read_buf->data[0]), frag_len);
                    frame_offset += frag_len;

                    if (read_buf->eoe_hdr.last_fragment != 0u) {
                        if (pec->tun_fd > 0) {
#if LIBETHERCAT_BUILD_POSIX == 1
                            write(pec->tun_fd, eth_frame->frame_data, eth_frame->frame_size);
#endif
                            pool_put(&slv->mbx.eoe.eth_frames_free_pool, p_eth_entry);
                        } else {
                            // put in receive pool, nobody cared so far
                            pool_put(&slv->mbx.eoe.eth_frames_recv_pool, p_eth_entry);
                        }

                        ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
                        p_entry = NULL;
                        p_eth_entry = NULL;
                        break;
                    }

                    ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
                    p_entry = NULL;

                    ec_eoe_wait(pec, slave, &p_entry); 
                }
            } else {
                if (pec->tun_fd > 0) {
                    eoe_debug_print("EOE_RECV", "recv eth frame", eth_frame->frame_data, frame_offset);

#if LIBETHERCAT_BUILD_POSIX == 1
                    write(pec->tun_fd, eth_frame->frame_data, frame_offset);
#endif
                    pool_put(&slv->mbx.eoe.eth_frames_free_pool, p_eth_entry);
                } else {
                    // put in receive pool, nobody cared so far.
                    pool_put(&slv->mbx.eoe.eth_frames_recv_pool, p_eth_entry);
                }

                p_eth_entry = NULL;
            }
        }

    }

    if (p_eth_entry != NULL) {
        pool_put(&slv->mbx.eoe.eth_frames_recv_pool, p_eth_entry);
    }
        
    if (p_entry != NULL) {        
        ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
    }

    return ret;
}

#if LIBETHERCAT_BUILD_POSIX == 1

//! \brief Handler thread for tap interface
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 */
static void ec_eoe_tun_handler(ec_t *pec) {
    assert(pec != NULL);

    eth_frame_t tmp_frame;

    while (pec->tun_running == 1) {
        int ret;
        fd_set rd_set;

        FD_ZERO(&rd_set);
        FD_SET(pec->tun_fd, &rd_set);

        struct timeval tv = {0, 100000};   // sleep for 100 ms
        ret = select(pec->tun_fd + 1, &rd_set, NULL, NULL, &tv);
        int local_errno = errno;

        if ((ret < 0) && (local_errno == EINTR)){
            (void)printf("select returned %d, errno %d\n", ret, errno);
            continue;
        }

        if (ret < 0) {
            perror("select()");
        } else {
            if (FD_ISSET(pec->tun_fd, &rd_set) != 0) {            
                int rd = read(pec->tun_fd, &tmp_frame.frame_data[0], sizeof(tmp_frame.frame_data));
                if (rd > 0) {
                    // simple switch here 
                    eoe_debug_print("EOE_TUN_HANDLER", "got eth frame", tmp_frame.frame_data, rd);

                    static const osal_uint8_t broadcast_mac[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
                    osal_uint8_t *dst_mac = &tmp_frame.frame_data[0];
                    int is_broadcast = 0;
                    if (memcmp(broadcast_mac, dst_mac, 6) == 0) {
                        is_broadcast = 1;
                    }

                    for (osal_uint16_t slave = 0; slave < pec->slave_cnt; ++slave) {
                        ec_slave_ptr(slv, pec, slave);
                        if (    !slv->eoe.use_eoe || 
                                (slv->act_state == EC_STATE_INIT)   ||
                                (slv->act_state == EC_STATE_BOOT)) {
                            continue;
                        }

                        if (
                                is_broadcast || 
                                (slv->eoe.mac && (memcmp(slv->eoe.mac, dst_mac, 6) == 0))   ) {
                            ec_log(100, "EOE_TUN_HANDLER", "slave %2d: sending eoe frame\n", slave);
                            (void)ec_eoe_send_frame(pec, slave, tmp_frame.frame_data, rd);

                            if (!is_broadcast) {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

static void *ec_eoe_tun_handler_wrapper(void *arg) {
    // cppcheck-suppress misra-c2012-11.5
    ec_t *pec = arg;
    ec_eoe_tun_handler(pec);
    return NULL;
}

#endif

// setup tun interface
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 */
int ec_eoe_setup_tun(ec_t *pec) {
    assert(pec != NULL);

    int ret = EC_OK;

#if LIBETHERCAT_BUILD_POSIX == 1
    int s;
    struct ifreq ifr;
    (void)memset(&ifr, 0, sizeof(ifr));

    // open tun device
    pec->tun_fd = open("/dev/net/tun", O_RDWR);
    if (pec->tun_fd == -1) {
        ec_log(1, "EOE_SETUP_TUN", "could not open /dev/net/tun\n");
        ret = EC_ERROR_UNAVAILABLE;
    } 

    if (ret == EC_OK) {
        ifr.ifr_flags = IFF_TAP | IFF_NO_PI; 

        if (ioctl(pec->tun_fd, TUNSETIFF, (void *)&ifr) != 0) {
            ec_log(1, "EOE_SETUP_TUN", "could not request tun/tap device\n");
            close(pec->tun_fd);
            pec->tun_fd = 0;
            ret = EC_ERROR_UNAVAILABLE;
        } 
    }

    if (ret == EC_OK) {
        ec_log(10, "EOE_SETUP_TUN", "using interface %s\n", ifr.ifr_name);

        // Create a socket
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0) {
            perror("socket");
            ret = EC_ERROR_UNAVAILABLE;
        } 
    }

    if (ret == EC_OK) {
        if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0) { // Get interface flags
            perror("cannot get interface flags");
            ret = EC_ERROR_UNAVAILABLE;
        } 
    }

    if (ret == EC_OK) {
        // Turn on interface
        ifr.ifr_flags |= IFF_UP;
        if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0) {
            (void)fprintf(stderr, "ifup: failed ");
            perror(ifr.ifr_name);
            ret = EC_ERROR_UNAVAILABLE;
        } 
    }

    if (ret == EC_OK) {
        // Set interface address
        struct sockaddr_in  my_addr;
        bzero((osal_char_t *) &my_addr, sizeof(my_addr));
        my_addr.sin_family = AF_INET;
        my_addr.sin_addr.s_addr = htonl(pec->tun_ip);
        (void)memcpy(&ifr.ifr_addr, &my_addr, sizeof(struct sockaddr));

        if (ioctl(s, SIOCSIFADDR, &ifr) < 0) {
            (void)fprintf(stderr, "Cannot set IP address. ");
            perror(ifr.ifr_name);
            ret = EC_ERROR_UNAVAILABLE;
        } else {
            pec->tun_running = 1;
            osal_task_attr_t attr;
            attr.priority = 5;
            attr.affinity = 0xFF;
            (void)strcpy(&attr.task_name[0], "ecat.tun");
            osal_task_create(&pec->tun_tid, &attr, ec_eoe_tun_handler_wrapper, pec);
        }
    }
#endif

    return ret;
}

// Destroy tun interface
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 */
void ec_eoe_destroy_tun(ec_t *pec) {
    assert(pec != NULL);

    if (pec->tun_running == 1) {
        pec->tun_running = 0;
#if LIBETHERCAT_BUILD_POSIX == 1
        osal_task_join(&pec->tun_tid, NULL);

        close(pec->tun_fd);
        pec->tun_fd = 0;
#endif
    }
}

