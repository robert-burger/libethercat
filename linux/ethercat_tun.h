/**
 * \file ethercat_tun.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 2026-04-15
 *
 * \brief Tun network device for EtherCAT network device.
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

#ifndef ETHERCAT_TUN__H
#define ETHERCAT_TUN__H

#include <linux/netdevice.h>
#include <linux/cdev.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/poll.h>
                                //
#define TUN_DEV_NAME_MAX_LENGTH		16u

// TUN-Device-Structure
struct tun_dev {
    struct net_device *dev;
    struct cdev cdev;
    char name[TUN_DEV_NAME_MAX_LENGTH];

    struct sk_buff_head rx_queue;
    wait_queue_head_t rx_wait;
};

/**
 * @brief Create EtherCAT tun device
 *
 * @param[in]   tun_dev     Pointer to tun_dev structure.
 * @param[in]   minor       tun device minor number.
 * @param[in]   dev_addr    Device MAC address (6-Byte!!)
 * @return 0 on success, -1 on error.
 */
int ethercat_tun_device_create(struct tun_dev *tun_dev, int minor, const unsigned char *dev_addr);

/**
 * @brief Destroy EtherCAT tun device
 *
 * @param[in]   tun_dev     Pointer to tun_dev structure.
 */
void ethercat_tun_device_destroy(struct tun_dev *tun_dev);

/**
 * @brief tun device initializraion
 */
int ethercat_tun_init(void);

/**
 * @brief tun device deinitializraion
 */
void ethercat_tun_exit(void);

#endif // ETHERCAT_TUN__H
 
