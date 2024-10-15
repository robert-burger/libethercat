/**
 * \file ethercat_device.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 2023-03-23
 *
 * \brief Character device for EtherCAT network device.
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

#ifndef ETHERCAT_DEVICE__H
#define ETHERCAT_DEVICE__H

#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/device.h>
#include <linux/swait.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ioctl.h>

/* Structure to hold EtherCAT char device
 */
struct ethercat_device {
    struct cdev cdev;                       //! \brief Linux character device.
    struct device *dev;                     //! \brief Linux device node in filesystem.
    unsigned minor;                         //! \brief Assigned device minor number.
    struct swait_queue_head ir_queue;       //! \brief Waitqueue for irq mode.

    struct net_device *net_dev;             //! \brief Assigned network hardware device.

    uint8_t link_state;
    unsigned int poll_mask;

    // internal ring buffer with socket buffers to be sent on network device.
#define EC_TX_RING_SIZE 0x100
    struct sk_buff *tx_skb[EC_TX_RING_SIZE];
    unsigned int tx_skb_index_next;

    // internal ring buffer with socket buffers containing received EtherCAT frames.
#define EC_RX_RING_SIZE 0x100
    struct sk_buff *rx_skb[EC_RX_RING_SIZE];
    unsigned int rx_skb_index_last_recv;
    unsigned int rx_skb_index_last_read;

    bool ethercat_polling;                  //! \brief EtherCAT polling mode (no irq's)
    uint64_t rx_timeout_ns;                 //! \brief Timeout in polling mode.

    // EtherCAT monitor device 
    bool monitor_enabled;                   //! \brief Monitor device enabled.
    struct net_device *monitor_dev;         //! \brief Monitor device net_dev.
    struct net_device_stats monitor_stats;  //! \brief Monitor device statistics.
};

//! \brief Create an characted device node for provided network device.
/*!
 * \param[in]   net_dev     Network device to attach.
 * \return Pointer to newly created EtherCAT device on success.
 */
struct ethercat_device *ethercat_device_create(struct net_device *net_dev);

//! \brief Destructs an EtherCAT device.
/*!
 * \param[in]   ecat_dev    Pointer to EtherCAT device to destruct.
 * \return 0 on success.
 */
int ethercat_device_destroy(struct ethercat_device *ecat_dev);

//! \brief Receive function called from network device if a frame was received.
/*!
 * \param[in]   ecat_dev    Pointer to EtherCAT device.
 * \param[in]   data        Pointer to memory containing the beginning of the EtherCAT frame.
 * \param[in]   size        Size of EtherCAT frame.
 */
void ethercat_device_receive(struct ethercat_device *ecat_dev, const void *data, size_t size);

//! \brief Set link status of EtherCAT device.
/*!
 * \param[in]   ecat_dev    Pointer to EtherCAT device.
 * \param[in]   link        New link state.
 */
void ethercat_device_set_link(struct ethercat_device *ecat_dev, bool link);

#endif 

