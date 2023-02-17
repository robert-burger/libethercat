/**
 * \file hw.h
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
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBETHERCAT_HW_H
#define LIBETHERCAT_HW_H

#include <libosal/task.h>
#include <libosal/types.h>
#include <libosal/mutex.h>

#include <libethercat/config.h>
#include <libethercat/pool.h>
#include <libethercat/datagram.h>

#if LIBETHERCAT_BUILD_PIKEOS == 1
#include <vm_file_types.h>
#include <drv/sbuf_hdr.h>
#endif

#define ETH_P_ECAT      0x88A4

// forward decl
struct ec;

//! hardware structure
typedef struct hw {
    struct ec *pec;

    int sockfd;                     //!< raw socket file descriptor
    osal_uint32_t mtu_size;              //!< mtu size

    // receiver thread settings
    osal_task_t rxthread;           //!< receiver thread handle
    int rxthreadrunning;            //!< receiver thread running flag

    osal_mutex_t hw_lock;           //!< transmit lock

    pool_t tx_high;                 //!< high priority datagrams
    pool_t tx_low;                  //!< low priority datagrams

    pool_entry_t *tx_send[256];     //!< sent datagrams

#if LIBETHERCAT_BUILD_DEVICE_SOCK_RAW_MMAPED == 1
    int mmap_packets;
    osal_char_t *rx_ring;                  //!< kernel mmap receive buffers
    osal_char_t *tx_ring;                  //!< kernel mmap send buffers

    off_t rx_ring_offset;
    off_t tx_ring_offset;

#define ETH_FRAME_LEN   0x1518
    osal_uint8_t recv_frame[ETH_FRAME_LEN];
#elif LIBETHERCAT_BUILD_DEVICE_SOCK_RAW_LEGACY == 1
#define ETH_FRAME_LEN   0x1518
    osal_uint8_t send_frame[ETH_FRAME_LEN];
    osal_uint8_t recv_frame[ETH_FRAME_LEN];
#elif LIBETHERCAT_BUILD_DEVICE_FILE == 1
#define ETH_FRAME_LEN   0x1518
    osal_uint8_t send_frame[ETH_FRAME_LEN];
    osal_uint8_t recv_frame[ETH_FRAME_LEN];
#elif LIBETHERCAT_BUILD_PIKEOS == 1
    vm_file_desc_t fd;
    drv_sbuf_desc_t sbuf;
#define ETH_FRAME_LEN   0x1518
    osal_uint8_t send_frame[ETH_FRAME_LEN];
    osal_uint8_t recv_frame[ETH_FRAME_LEN];
#endif
} hw_t;   

#ifdef __cplusplus
extern "C" {
#endif

//! open a new hw
/*!
 * \param phw return hw 
 * \param devname ethernet device name
 * \param prio receive thread prio
 * \param cpumask receive thread cpumask
 * \return 0 or negative error code
 */
int hw_open(hw_t *phw, struct ec *pec, const osal_char_t *devname, int prio, int cpumask);

//! destroys a hw
/*!
 * \param phw hw handle
 * \return 0 or negative error code
 */
int hw_close(hw_t *phw);

//! start sending queued ethercat datagrams
/*!
 * \param phw hardware handle
 * \return 0 or error code
 */
int hw_tx(hw_t *phw);

//! Process a received EtherCAT frame
/*!
 * \param[in]   phw     Pointer to hw handle.
 * \param[in]   pframe  Pointer to received EtherCAT frame.
 */
void hw_process_rx_frame(hw_t *phw, ec_frame_t *pframe);

//! Opens EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   devname     Null-terminated string to EtherCAT hw device name.
 *
 * \return 0 or negative error code
 */
int hw_device_open(hw_t *phw, const osal_char_t *devname);

//! Receive a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 *
 * \return 0 or negative error code
 */
int hw_device_recv(hw_t *phw);

//! Send a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   pframe      Pointer to frame buffer.
 *
 * \return 0 or negative error code
 */
int hw_device_send(hw_t *phw, ec_frame_t *pframe);

//! Get a free tx buffer from underlying hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   ppframe     Pointer to return frame buffer pointer.
 *
 * \return 0 or negative error code
 */
int hw_device_get_tx_buffer(hw_t *phw, ec_frame_t **ppframe);

#ifdef __cplusplus
}
#endif

#endif // LIBETHERCAT_HW_H

