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

#include "config.h"

#include "libethercat/ec.h"
#include "libethercat/eoe.h"
#include "libethercat/timer.h"
#include "libethercat/error_codes.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

// forward declarations
void ec_eoe_process_recv(ec_t *pec, uint16_t slave);

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

typedef struct eth_frame {
    size_t frame_size;
    uint8_t frame_data[1518];
} eth_frame_t;

void eoe_debug_print(char *msg, uint8_t *frame, size_t frame_len) {
#define EOE_DEBUG_BUFFER_SIZE   1024
    static char eoe_debug_buffer[EOE_DEBUG_BUFFER_SIZE];
    
    int pos = snprintf(eoe_debug_buffer, EOE_DEBUG_BUFFER_SIZE, "%s: ", msg);
    for (unsigned i = 0; (i < frame_len) && (pos < EOE_DEBUG_BUFFER_SIZE); ++i) {
        pos += snprintf(eoe_debug_buffer+pos, EOE_DEBUG_BUFFER_SIZE-pos, "%02X", frame[i]);
    }

    ec_log(100, __func__, "%s\n", eoe_debug_buffer);
}

//! initialize EoE structure 
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 * \param[in] slave         Number of ethercat slave. this depends on 
 *                          the physical order of the ethercat slaves 
 *                          (usually the n'th slave attached).
 */
void ec_eoe_init(ec_t *pec, uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);

    pthread_mutex_init(&slv->mbx.eoe.lock, NULL);

    pool_open(&slv->mbx.eoe.recv_pool, 0, 1518);
    pool_open(&slv->mbx.eoe.response_pool, 0, 1518);
    pool_open(&slv->mbx.eoe.eth_frames_free_pool, 128, sizeof(eth_frame_t));
    pool_open(&slv->mbx.eoe.eth_frames_recv_pool, 0, sizeof(eth_frame_t));

    sem_init(&slv->mbx.eoe.send_sync, 0, 0);
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
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    
    pthread_mutex_lock(&slv->mbx.eoe.lock);

    sem_destroy(&slv->mbx.eoe.send_sync);
    pool_close(slv->mbx.eoe.eth_frames_recv_pool);
    pool_close(slv->mbx.eoe.eth_frames_free_pool);
    pool_close(slv->mbx.eoe.recv_pool);
    
    pthread_mutex_unlock(&slv->mbx.eoe.lock);
    pthread_mutex_destroy(&slv->mbx.eoe.lock);
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
static void ec_eoe_wait_response(ec_t *pec, uint16_t slave, pool_entry_t **pp_entry) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(pp_entry != NULL);

    ec_slave_ptr(slv, pec, slave);

    ec_mbx_sched_read(pec, slave);

    ec_timer_t timeout;
    ec_timer_init(&timeout, EC_DEFAULT_TIMEOUT_MBX);

    pool_get(slv->mbx.eoe.response_pool, pp_entry, &timeout);
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
static void ec_eoe_wait(ec_t *pec, uint16_t slave, pool_entry_t **pp_entry) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(pp_entry != NULL);

    ec_slave_ptr(slv, pec, slave);

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
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(p_entry != NULL);

    ec_slave_ptr(slv, pec, slave);
    
    ec_eoe_request_t *write_buf = (ec_eoe_request_t *)(p_entry->data);
    ec_log(100, __func__, "slave %2d: got eoe frame type 0x%X\n", slave, 
            write_buf->eoe_hdr.frame_type);
    
    if (write_buf->eoe_hdr.frame_type == EOE_FRAME_TYPE_SET_IP_ADDRESS_RESPONSE) {
        pool_put(slv->mbx.eoe.response_pool, p_entry);
    } else {
        pool_put(slv->mbx.eoe.recv_pool, p_entry);

        if (write_buf->eoe_hdr.last_fragment) {
            ec_log(100, __func__, "slave %2d: was last fragment\n", slave);
            ec_eoe_process_recv(pec, slave);
        }
    }
}

// read coe sdo 
int ec_eoe_set_ip_parameter(ec_t *pec, uint16_t slave, uint8_t *mac,
        uint8_t *ip_address, uint8_t *subnet, uint8_t *gateway, 
        uint8_t *dns, char *dns_name) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    int ret = 0;
    ec_slave_ptr(slv, pec, slave);
    ec_mbx_check(EC_EEPROM_MBX_EOE, EoE);

    pthread_mutex_lock(&slv->mbx.eoe.lock);

    ec_log(10, __func__, "slave %2d: set ip parameter\n", slave);

    pool_entry_t *p_entry;
    ec_mbx_get_free_send_buffer(pec, slave, p_entry, NULL, &slv->mbx.eoe.lock);
    memset(p_entry->data, 0, p_entry->data_size);

    ec_eoe_set_ip_parameter_request_t *write_buf = (ec_eoe_set_ip_parameter_request_t *)(p_entry->data);
    
    // mailbox header
    // (mbxhdr (6) - mbxhdr.length (2)) + eoehdr (4) + siphdr (4)
    write_buf->mbx_hdr.length    = 8;
    write_buf->mbx_hdr.mbxtype   = EC_MBX_EOE;
    // eoe header
    write_buf->eoe_hdr.frame_type         = EOE_FRAME_TYPE_SET_IP_ADDRESS_REQUEST;
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
    ec_mbx_enqueue_tail(pec, slave, p_entry);

    // wait for answer
    for (ec_eoe_wait_response(pec, slave, &p_entry); p_entry; ec_eoe_wait_response(pec, slave, &p_entry)) {
        ec_eoe_set_ip_parameter_response_t *read_buf = (ec_eoe_set_ip_parameter_response_t *)(p_entry->data);

        ret = read_buf->sip_hdr.result;
        ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
        break;
    }

    pthread_mutex_unlock(&slv->mbx.eoe.lock);

    return ret;
}
    
void ec_eoe_send_sync(void *user_arg, struct pool_entry *p) {
    ec_mbx_t *pmbx = (ec_mbx_t *)user_arg;
    
    sem_post(&pmbx->pec->slaves[pmbx->slave].mbx.eoe.send_sync);
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
int ec_eoe_send_frame(ec_t *pec, uint16_t slave, uint8_t *frame, 
        size_t frame_len) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(frame != NULL);

    int ret = 0;
    ec_slave_ptr(slv, pec, slave);
    ec_mbx_check(EC_EEPROM_MBX_EOE, EoE);

    size_t max_frag_len = (slv->sm[MAILBOX_WRITE].len - sizeof(ec_mbx_header_t) - sizeof(ec_eoe_header_t));
    ALIGN_32BIT_BLOCKS(max_frag_len);
    off_t frame_offset = 0;
    int frag_number = 0;

    pthread_mutex_lock(&slv->mbx.eoe.lock);

    do {
        size_t frag_len = min(frame_len - frame_offset, max_frag_len);

        // get mailbox buffer to write frame fragment request
        pool_entry_t *p_entry;
        ec_timer_t timeout;
        ec_timer_gettime(&timeout);
        timeout.sec += 10;
        ec_mbx_get_free_send_buffer(pec, slave, p_entry, &timeout, &slv->mbx.eoe.lock);

        // send sync callback
        p_entry->user_cb = ec_eoe_send_sync;
        p_entry->user_arg = &slv->mbx;

        memset(p_entry->data, 0, p_entry->data_size);
        ec_eoe_request_t *write_buf = (ec_eoe_request_t *)(p_entry->data);

        ec_log(100, __func__, "slave %2d: sending eoe fragment %d\n", slave, frag_number);
        // mailbox header
        // (mbxhdr (6) - mbxhdr.length (2)) + eoehdr (8) + sdohdr (4)
        write_buf->mbx_hdr.length           = 4 + frag_len;
        write_buf->mbx_hdr.mbxtype          = EC_MBX_EOE;
        // eoe header
        write_buf->eoe_hdr.frame_type       = EOE_FRAME_TYPE_REQUEST;
        write_buf->eoe_hdr.fragment_number  = frag_number;

        if (frag_len < max_frag_len) { write_buf->eoe_hdr.last_fragment  = 0x01; } 
        else                         { write_buf->eoe_hdr.last_fragment  = 0x00; }
        
        if (frag_number == 0)        { write_buf->eoe_hdr.complete_size  = (frame_len + 31) >> 5; }
        else                         { write_buf->eoe_hdr.complete_size  = (frame_offset) >> 5;   }
    
        // copy fragment
        memcpy(&write_buf->data.bdata[0], &frame[frame_offset], frag_len);
        frame_offset += frag_len;
        frag_number++;

        // send request
        ec_mbx_enqueue_tail(pec, slave, p_entry);
        sem_wait(&slv->mbx.eoe.send_sync);
    } while(frame_offset < frame_len);

    pthread_mutex_unlock(&slv->mbx.eoe.lock);
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
void ec_eoe_process_recv(ec_t *pec, uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    ec_slave_ptr(slv, pec, slave);
    pool_entry_t *p_entry = NULL, *p_eth_entry = NULL;
    pool_get(slv->mbx.eoe.eth_frames_free_pool, &p_eth_entry, NULL);
    eth_frame_t *eth_frame = (eth_frame_t *)(p_eth_entry->data);
    
    // get first received EoE frame, should be start of ethernet frame
    pool_get(slv->mbx.eoe.recv_pool, &p_entry, NULL);
    if (!p_entry) { 
        pool_put(slv->mbx.eoe.eth_frames_free_pool, p_eth_entry);
        return; 
    }
    
    ec_eoe_request_t *read_buf = (ec_eoe_request_t *)(p_entry->data);
    if (read_buf->eoe_hdr.fragment_number != 0) {
        ec_log(1, __func__, "slave %2d: first EoE fragment is not first fragment (got %d)\n",
                slave, read_buf->eoe_hdr.fragment_number);

        // proceed with next EoE message until queue is empty
        ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
        return ec_eoe_process_recv(pec, slave);
    }

    eth_frame->frame_size = read_buf->eoe_hdr.complete_size << 5;
    size_t frag_len       = read_buf->mbx_hdr.length - 4;
    off_t frame_offset    = 0;

    memcpy(&(eth_frame->frame_data[frame_offset]), &read_buf->data.bdata[0], frag_len);
    frame_offset += frag_len;

    if (!read_buf->eoe_hdr.last_fragment) {
        ec_mbx_return_free_recv_buffer(pec, slave, p_entry);

        // proceed with next fragment
        for (ec_eoe_wait(pec, slave, &p_entry); p_entry; ec_eoe_wait(pec, slave, &p_entry)) {
            ec_eoe_request_t *read_buf = (ec_eoe_request_t *)(p_entry->data);

            if (frame_offset != (read_buf->eoe_hdr.complete_size << 5)) {
                ec_log(1, __func__, "slave %d: frame offset mismatch %d != %d\n", 
                        slave, frame_offset, read_buf->eoe_hdr.complete_size << 5);
            }

            frag_len = read_buf->mbx_hdr.length - 4;
            memcpy(&(eth_frame->frame_data[frame_offset]), &(read_buf->data.bdata[0]), frag_len);
            frame_offset += frag_len;

            if (read_buf->eoe_hdr.last_fragment) {
                if (pec->tun_fd > 0) {
                    write(pec->tun_fd, eth_frame->frame_data, eth_frame->frame_size);
                    pool_put(slv->mbx.eoe.eth_frames_free_pool, p_eth_entry);
                } else {
                    ec_log(10, __func__, "slave %2d: got EoE frame, but there's no tun/tap device configured!\n", slave);
                }
                    
                pool_put(slv->mbx.eoe.eth_frames_recv_pool, p_eth_entry);

                ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
                p_eth_entry = NULL;
                break;
            }

            if (p_entry) {        
                ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
            }
        }
    } else {
        if (pec->tun_fd > 0) {
            eoe_debug_print("recv eth frame", eth_frame->frame_data, frame_offset);

            write(pec->tun_fd, eth_frame->frame_data, frame_offset);
            pool_put(slv->mbx.eoe.eth_frames_free_pool, p_eth_entry);
        } else {
            ec_log(10, __func__, "slave %2d: got EoE frame, but there's no tun/tap device configured!\n", slave);
        }
        
        pool_put(slv->mbx.eoe.eth_frames_free_pool, p_eth_entry);
    }
        
    if (p_entry) {        
        ec_mbx_return_free_recv_buffer(pec, slave, p_entry);
    }

    return;
}

//! \brief Handler thread for tap interface
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 */
void ec_eoe_tun_handler(ec_t *pec) {
    assert(pec != NULL);

    eth_frame_t tmp_frame;
    struct sched_param param;
    int policy;

    // thread settings$
    if (pthread_getschedparam(pthread_self(), &policy, &param) != 0) {
        ec_log(1, __func__, "error on pthread_getschedparam %s\n", strerror(errno));
    } else {
        policy = SCHED_FIFO;
        param.sched_priority = sched_get_priority_min(policy);
        if (pthread_setschedparam(pthread_self(), policy, &param) != 0)
            ec_log(1, __func__, "error on pthread_setschedparam %s\n", strerror(errno));
    }

    while (pec->tun_running) {
        int ret;
        fd_set rd_set;

        FD_ZERO(&rd_set);
        FD_SET(pec->tun_fd, &rd_set);

        struct timeval tv = {0, 100000};   // sleep for 100 ms
        ret = select(pec->tun_fd + 1, &rd_set, NULL, NULL, &tv);

        if (ret < 0 && errno == EINTR){
            printf("select returned %d, errno %d\n", ret, errno);
            continue;
        }

        if (ret < 0) {
            perror("select()");
            exit(1);
        }

        if (FD_ISSET(pec->tun_fd, &rd_set)) {            
            int rd = read(pec->tun_fd, &tmp_frame.frame_data[0], sizeof(tmp_frame.frame_data));
            if (rd > 0) {
                // simple switch here 
                eoe_debug_print("got eth frame", tmp_frame.frame_data, rd);

                static const uint8_t broadcast_mac[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
                uint8_t *dst_mac = &tmp_frame.frame_data[0];
                int is_broadcast = (memcmp(broadcast_mac, dst_mac, 6) == 0);

                for (uint16_t slave = 0; slave < pec->slave_cnt; ++slave) {
                    ec_slave_ptr(slv, pec, slave);
                    if (    !slv->eoe.use_eoe || 
                            (slv->act_state == EC_STATE_INIT)   ||
                            (slv->act_state == EC_STATE_BOOT)) {
                        continue;
                    }

                    if (
                            is_broadcast || 
                            (slv->eoe.mac && (memcmp(slv->eoe.mac, dst_mac, 6) == 0))   ) {
                        ec_log(100, __func__, "slave %2d: sending eoe frame\n", slave);
                        ec_eoe_send_frame(pec, slave, tmp_frame.frame_data, rd);

                        if (!is_broadcast) {
                            break;
                        }
                    }
                }
            }
        }
    }
}

void *ec_eoe_tun_handler_wrapper(void *arg) {
    ec_t *pec = arg;
    ec_eoe_tun_handler(pec);
    return NULL;
}

// setup tun interface
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 */
int ec_eoe_setup_tun(ec_t *pec) {
    assert(pec != NULL);

    // open tun device
    pec->tun_fd = open("/dev/net/tun", O_RDWR);
    if (pec->tun_fd == -1) {
        ec_log(1, __func__, "could not open /dev/net/tun\n");
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI; 

    if (ioctl(pec->tun_fd, TUNSETIFF, (void *)&ifr)) {
        ec_log(1, __func__, "could not request tun/tap device\n");
        close(pec->tun_fd);
        pec->tun_fd = 0;
        return -1;
    }

    ec_log(10, __func__, "using interface %s\n", ifr.ifr_name);

    int s;
    // Create a socket
    if ( (s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    // Get interface flags
    if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0) {
        perror("cannot get interface flags");
        exit(1);
    }

    // Turn on interface
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0) {
        fprintf(stderr, "ifup: failed ");
        perror(ifr.ifr_name);
        exit(1);
    }

    // Set interface address
    struct sockaddr_in  my_addr;
    bzero((char *) &my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = htonl(pec->tun_ip);//inet_network("192.168.2.1"));
    memcpy(&ifr.ifr_addr, &my_addr, sizeof(struct sockaddr));

    if (ioctl(s, SIOCSIFADDR, &ifr) < 0) {
        fprintf(stderr, "Cannot set IP address. ");
        perror(ifr.ifr_name);
        exit(1);
    }

    pec->tun_running = 1;
    pthread_create(&pec->tun_tid, NULL, ec_eoe_tun_handler_wrapper, pec);
    return 0;
}

// Destroy tun interface
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 */
void ec_eoe_destroy_tun(ec_t *pec) {
    assert(pec != NULL);

    if (!pec->tun_running) {
        return;
    }

    pec->tun_running = 0;
    pthread_join(pec->tun_tid, NULL);

    close(pec->tun_fd);
    pec->tun_fd = 0;
}

