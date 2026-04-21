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

#include "ethercat_tun.h"
#include "ethercat_monitor.h"

/**
 * Structure to hold EtherCAT char device
 */
struct ethercat_device {
    struct cdev cdev;                       //! \brief Linux character device.
    struct device *dev;                     //! \brief Linux device node in filesystem.
    unsigned minor;                         //! \brief Assigned device minor number.
    struct swait_queue_head rx_wait;        //! \brief Waitqueue for irq mode.
    spinlock_t queue_lock;
    struct sk_buff_head skb_queue_free;     //! \brief Free sk_buff for send or receive.
    struct sk_buff_head rx_queue;           //! \brief sk_buff queeu with received skb's.

    struct net_device *net_dev;             //! \brief Assigned network hardware device.

    uint8_t link_state;                     //! \brief Identifier if we have link.
    unsigned int poll_mask;                 //! \brief Receive poll mask.

    bool ethercat_polling;                  //! \brief EtherCAT polling mode (no irq's)
    uint64_t rx_timeout_ns;                 //! \brief Timeout in polling mode.

    struct monitor_dev monitor_dev;         //! \brief Monitor device (network device).
    struct tun_dev tun_dev;                 //! \brief TUN device for virtual network (e.g EoE).
};

//! \brief Create an characted device node for provided network device.
/*!
 * \param[in]   net_dev     Network device to attach.
 * \return Pointer to newly created EtherCAT device on success.
 */
struct ethercat_device *ethercat_device_create(struct net_device *net_dev);

//! \brief Sent finished function called from network device if a frame was sent.
/*!
 * \param[in]   ecat_dev    Pointer to EtherCAT device.
 * \param[in]   skb         Pointer to socket buffer to free.
 */
void ethercat_device_sent_finished(struct ethercat_device *ecat_dev, struct sk_buff *skb);

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

//! \brief Kernel func, which is not exported. Collected via kallsym.
typedef int (*fcn_devinet_ioctl_t)(struct net *net, unsigned int cmd, void __user *arg);
extern fcn_devinet_ioctl_t fcn_devinet_ioctl;

#endif 

