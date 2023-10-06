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

#ifndef LIBETHERCAT_HW_H
#define LIBETHERCAT_HW_H

#include <libosal/task.h>
#include <libosal/types.h>
#include <libosal/mutex.h>

#include <libethercat/config.h>
#include <libethercat/pool.h>
#include <libethercat/datagram.h>

#if LIBETHERCAT_BUILD_DEVICE_PIKEOS == 1
#include <vm_file_types.h>
#include <drv/sbuf_hdr.h>
#endif

/** \defgroup hardware_group HW
 *
 * This modules contains main EtherCAT hardware functions.
 *
 * @{
 */

#define ETH_P_ECAT      (0x88A4)        //!< \brief Ethertype for EtherCAT.

// forward decl
struct ec;
struct hw;

//! Opens EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   devname     Null-terminated string to EtherCAT hw device name.
 *
 * \return 0 or negative error code
 */
typedef int (*hw_device_open_t)(struct hw *phw, const osal_char_t *devname);

//! Receive a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 *
 * \return 0 or negative error code
 */
typedef int (*hw_device_recv_t)(struct hw *phw);

//! Send a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   pframe      Pointer to frame buffer.
 *
 * \return 0 or negative error code
 */
typedef int (*hw_device_send_t)(struct hw *phw, ec_frame_t *pframe);

//! Doing internal stuff when finished sending frames
/*!
 * \param[in]   phw         Pointer to hw handle.
 */
typedef void (*hw_device_send_finished_t)(struct hw *phw);

//! Get a free tx buffer from underlying hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   ppframe     Pointer to return frame buffer pointer.
 *
 * \return 0 or negative error code
 */
typedef int (*hw_device_get_tx_buffer_t)(struct hw *phw, ec_frame_t **ppframe);

typedef struct hw_device {
    hw_device_open_t open;
    hw_device_recv_t recv;
    hw_device_send_t send;
    hw_device_send_finished_t send_finished;
    hw_device_get_tx_buffer_t get_tx_buffer;
} hw_device_t;

//! hardware structure
typedef struct hw {
    struct ec *pec;                 //!< Pointer to EtherCAT master structure.

    int sockfd;                     //!< raw socket file descriptor
    osal_uint32_t mtu_size;         //!< mtu size

    // receiver thread settings
    osal_task_t rxthread;           //!< receiver thread handle
    int rxthreadrunning;            //!< receiver thread running flag

    osal_mutex_t hw_lock;           //!< transmit lock

    pool_t tx_high;                 //!< high priority datagrams
    pool_t tx_low;                  //!< low priority datagrams

    pool_entry_t *tx_send[256];     //!< sent datagrams

    osal_size_t bytes_sent;         //!< \brief Bytes currently sent.
    osal_size_t bytes_last_sent;    //!< \brief Bytes last sent.
    osal_timer_t next_cylce_start;  //!< \brief Next cycle start time.

    hw_device_recv_t recv;
    hw_device_send_t send;
    hw_device_send_finished_t send_finished;
    hw_device_get_tx_buffer_t get_tx_buffer;

#define ETH_FRAME_LEN   0x1518
    osal_uint8_t send_frame[ETH_FRAME_LEN]; //!< \brief Static send frame.
    osal_uint8_t recv_frame[ETH_FRAME_LEN]; //!< \brief Static receive frame.
    osal_bool_t polling_mode;               //!< \brief Special interrupt-less polling-mode flag.
    
    int mmap_packets;               //!< \brief Doing mmap packets.
    osal_char_t *rx_ring;           //!< kernel mmap receive buffers
    osal_char_t *tx_ring;           //!< kernel mmap send buffers

    off_t rx_ring_offset;           //!< \brief Offset in RX ring.
    off_t tx_ring_offset;           //!< \brief Offset in TX ring.

#if LIBETHERCAT_BUILD_DEVICE_PIKEOS == 1
    vm_file_desc_t fd;                      //!< \brief Driver file descriptor.
    drv_sbuf_desc_t sbuf;                   //!< \brief Driver SBUF descriptor.
#endif
} hw_t;                 //!< \brief Hardware struct type. 

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
int hw_tx_low(hw_t *phw);

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

#ifdef __cplusplus
}
#endif

/** @} */

#endif // LIBETHERCAT_HW_H

