/**
 * \file eoe.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 22 Nov 2016
 *
 * \brief EtherCAT eoe functions.
 *
 * Implementaion of the Ethernet over EtherCAT mailbox protocol
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

#ifndef LIBETHERCAT_VETH_H
#define LIBETHERCAT_VETH_H

#include "libosal/types.h"
#include "libosal/task.h"
#include "libethercat/pool.h"

#define EC_VETH_ETH_ALEN 6

typedef struct ec_veth {
    int fd;                     //!< tun device file descriptor
    osal_uint32_t ip;           //!< tun device ip addres
    osal_uint8_t mac[EC_VETH_ETH_ALEN];
    osal_task_t tid;            //!< tun device handler thread id.
    osal_bool_t running;        //!< tun device handler run flag.
} ec_veth_t;

typedef struct eth_frame {
    osal_size_t frame_size;
    osal_uint8_t frame_data[1518];
} eth_frame_t;

// forward declarations
struct ec;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Open exisiting TUN device like '/dev/ecat_tun0' created by ethercat_chardev.ko!
 *
 * @param[in]   pec             Pointer to ethercat master structure.
 * @param[in]   tun_dev_name    Name of tun character device file.
 * @param[in]   mac_addr        MAC address of master device.
 * @param[in]   ip_addr         IP address of master device.
 * @retval EC_OK                Opening and starting of tun device successfull
 * @retval EC_ERROR_UNAVAILABLE Tun device file could not be opened.
 */
int ec_veth_open_tun(
        struct ec *pec, 
        const char *tun_dev_name, 
        const uint8_t mac_addr[EC_VETH_ETH_ALEN], 
        const uint32_t ip_addr);

/**
 * @brief Close previously opened TUN device.
 *
 * @param[in]   pec             Pointer to ethercat master structure.
 */
void ec_veth_close_tun(struct ec *pec);

/**
 * @brief Process received Ethernet frame.
 *
 * @param[in]   pec         Pointer to EtherCAT Master structure.
 * @param[in]   buf         Buffer pointing to Ethernet frame.
 * @param[in]   len         Length of Ethernet frame.
 */
void ec_veth_process_frame(struct ec *pec, uint8_t *buf, size_t len);

/**
 * @brief Send Ethernet frame over tun.
 *
 * @param[in]   pec         Pointer to EtherCAT Master structure.
 * @param[in]   buf         Buffer pointing to Ethernet frame.
 * @param[in]   len         Length of Ethernet frame.
 */
int ec_veth_send_frame(struct ec *pec, uint8_t *buf, size_t len);

//! \brief Enqueue CoE message received from slave.
/*!
 * \param[in] pec       Pointer to ethercat master structure, 
 *                      which you got from \link ec_open \endlink.
 * \param[in] p_entry   Pointer to pool entry containing received
 *                      mailbox message from slave.
 */
void ec_mbx_gateway_enqueue(struct ec *pec, pool_entry_t *p_entry);

#ifdef __cplusplus
};
#endif

#endif // LIBETHERCAT_VETH_H

