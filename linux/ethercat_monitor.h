/**
 * \file ethercat_monitor.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 2026-04-15
 *
 * \brief Monitor network device for EtherCAT network device.
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

/**
 * Creates a network interface for monitoring purpose called ecat%d_monitor and registers it to
 * the linux network stack. Ensure that the interface is brought up by something like:
 *
 * $ ip link set up ecat0_monitor
 *
 * Then use the usual tools to log sent and received EtherCAT frames like tcpdump, wireshark, etc....
 *
 * WARNING: This should only be enabled for debugging purpose as it may allocate and free memory!!!
 */

#ifndef ETHERCAT_MONITOR__H
#define ETHERCAT_MONITOR__H

#include <linux/netdevice.h>

struct monitor_dev {
    bool enabled;                   		//! \brief Monitor device enabled.
    struct net_device *net_dev;         	//! \brief Monitor device net_dev.
    struct net_device_stats net_dev_stats;  	//! \brief Monitor device statistics.
};

/**
 * @brief Creates an EtherCAT monitor device
 *
 * @param[in]   monitor_dev    Pointer to EtherCAT monitor device to create.
 * @param[in]   minor          Device minor number.
 * @param[in]   dev_addr       Device MAC address (6-Byte!!)
 * @return 0 on success, -1 on error.
 */
int ethercat_monitor_create(struct monitor_dev *monitor_dev, u16 minor, const unsigned char *dev_addr);

/** 
 * @brief Destroys an EtherCAT monitor device
 *
 * @param[in]   monitor_dev    Pointer to EtherCAT monitor device to destruct.
 */
void ethercat_monitor_destroy(struct monitor_dev *monitor_dev);

/**
 * @brief Send an EtherCAT frame to the monitor device
 *
 * @param[in]   monitor_dev Pointer to EtherCAT monitor device to destruct.
 * @param[in]   data        Pointer to memory containing the beginning of the EtherCAT frame.
 * @param[in]   datalen     Size of EtherCAT frame.
 */
void ethercat_monitor_frame(struct monitor_dev *monitor_dev, const uint8_t *data, size_t datalen);

/**
 * @brief En- or disable monitor device.
 *
 * @param[in]   monitor_dev Pointer to EtherCAT monitor device to destruct.
 * @param[in]   enable      Set to 0 to disable, otherwise enable.
 */
void ethercat_monitor_enable(struct monitor_dev *monitor_dev, unsigned int enable);

#endif // ETHERCAT_MONITOR__H

