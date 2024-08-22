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

#define container_of(ptr, type, member) ({ \
        __typeof__( ((type *)0)->member ) *__mptr = (void *)(ptr); \
        (type *)( (char *)__mptr - offsetof(type,member) );})

/** \defgroup hardware_group HW
 *
 * This modules contains main EtherCAT hardware functions.
 *
 * @{
 */

#define ETH_P_ECAT      (0x88A4)        //!< \brief Ethertype for EtherCAT.

// forward decl
struct ec;
struct hw_common;

//! Flag to distinguish the pool types during processing
typedef enum pooltype {
    POOL_HIGH,
    POOL_LOW,
} pooltype_t; //!< \brief pool type struct type. 
              //
//! Receive a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 *
 * \return 0 or negative error code
 */
typedef int (*hw_device_recv_t)(struct hw_common *phw);

//! Send a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   pframe      Pointer to frame buffer.
 * \param[in]   pool_type   Pool type to distinguish between high and low prio frames.
 *
 * \return 0 or negative error code
 */
typedef int (*hw_device_send_t)(struct hw_common *phw, ec_frame_t *pframe, pooltype_t pool_type);

//! Doing internal stuff when finished sending frames
/*!
 * \param[in]   phw         Pointer to hw handle.
 */
typedef void (*hw_device_send_finished_t)(struct hw_common *phw);

//! Get a free tx buffer from underlying hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   ppframe     Pointer to return frame buffer pointer.
 *
 * \return 0 or negative error code
 */
typedef int (*hw_device_get_tx_buffer_t)(struct hw_common *phw, ec_frame_t **ppframe);

//! Close hardware layer
/*!
 * \param[in]   phw         Pointer to hw handle.
 *
 * \return 0 or negative error code
 */
typedef int (*hw_device_close_t)(struct hw_common *phw);

#define ETH_FRAME_LEN   0x1518

//! hardware structure
typedef struct hw_common {
    struct ec *pec;                 //!< Pointer to EtherCAT master structure.

    osal_uint32_t mtu_size;         //!< mtu size
    osal_mutex_t hw_lock;           //!< transmit lock

    pool_t tx_high;                 //!< high priority datagrams
    pool_t tx_low;                  //!< low priority datagrams

    pool_entry_t *tx_send[256];     //!< sent datagrams

    osal_size_t bytes_sent;         //!< \brief Bytes currently sent.
    osal_size_t bytes_last_sent;    //!< \brief Bytes last sent.
    osal_timer_t next_cylce_start;  //!< \brief Next cycle start time.

    hw_device_recv_t recv;                      //!< \biref Function to receive frame from device.
    hw_device_send_t send;                      //!< \brief Function to send frames via device.
    hw_device_send_finished_t send_finished;    //!< \brief Function to be called after frames were sent.
    hw_device_get_tx_buffer_t get_tx_buffer;    //!< \brief Function to retreave next TX buffer.
    hw_device_close_t close;                    //!< \brief Function to close hw layer.
} hw_common_t;                 //!< \brief Hardware struct type. 

#ifdef __cplusplus
extern "C" {
#endif

//! open a new hw
/*!
 * \param[in]   phw         Pointer to hw structure.
 * \param[in]   pec         Pointer to master structure.
 *
 * \return 0 or negative error code
 */
int hw_open(struct hw_common *phw, struct ec *pec);

//! destroys a hw
/*!
 * \param phw hw handle
 * \return 0 or negative error code
 */
int hw_close(struct hw_common *phw);

//! start sending queued ethercat datagrams
/*!
 * \param phw hardware handle
 * \return 0 or error code
 */
int hw_tx_low(struct hw_common *phw);

//! start sending queued ethercat datagrams
/*!
 * \param phw hardware handle
 * \return 0 or error code
 */
int hw_tx(struct hw_common *phw);

//! Process a received EtherCAT frame
/*!
 * \param[in]   phw     Pointer to hw handle.
 * \param[in]   pframe  Pointer to received EtherCAT frame.
 */
void hw_process_rx_frame(struct hw_common *phw, ec_frame_t *pframe);

#ifdef __cplusplus
}
#endif

/** @} */

#endif // LIBETHERCAT_HW_H

