/**
 * \file ethercat_monitor.c
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

#include <linux/version.h>

#include "ethercat_monitor.h"
#include "ethercat_device.h"

// forward declarations
static int ethercat_monitor_open(struct net_device *dev);
static int ethercat_monitor_stop(struct net_device *dev);
static int ethercat_monitor_tx(struct sk_buff *skb, struct net_device *dev);
static void ethercat_monitor_get_stats64(struct net_device *dev, struct rtnl_link_stats64 *stats);

//! Open Callback
/*
 * Nothing to open here.
 */
int ethercat_monitor_open(struct net_device *dev) {
    struct monitor_dev *monitor_dev = *(struct monitor_dev **)netdev_priv(dev);
    monitor_dev->enabled = true;
    return 0;
}

//! Close Callback
/*
 * Nothing to close here.
 */
int ethercat_monitor_stop(struct net_device *dev) {
    struct monitor_dev *monitor_dev = *(struct monitor_dev **)netdev_priv(dev);
    monitor_dev->enabled = false;
    return 0;
}

//! TX callback function
/*
 * Drop all frame someone wants to send to the monitor device from outside.
 */
int ethercat_monitor_tx(struct sk_buff *skb, struct net_device *dev) {
    struct monitor_dev *monitor_dev = *(struct monitor_dev **)netdev_priv(dev);
    dev_kfree_skb(skb);
    monitor_dev->net_dev_stats.tx_dropped++;
    return 0;
}

//! Get statistics callback
void ethercat_monitor_get_stats64(struct net_device *dev, struct rtnl_link_stats64 *stats) 
{
    struct monitor_dev *monitor_dev = *(struct monitor_dev **)netdev_priv(dev);

    // Fill stats from monitor driver's internal counters
    stats->rx_packets = monitor_dev->net_dev_stats.rx_packets;
    stats->tx_packets = monitor_dev->net_dev_stats.tx_packets;
    stats->rx_bytes   = monitor_dev->net_dev_stats.rx_bytes;
    stats->tx_bytes   = monitor_dev->net_dev_stats.tx_bytes;
    stats->tx_dropped = monitor_dev->net_dev_stats.tx_dropped;
    stats->rx_dropped = monitor_dev->net_dev_stats.rx_dropped;
}

//! Network device ops.
static const struct net_device_ops ethercat_monitor_netdev_ops = {
    .ndo_open = ethercat_monitor_open,
    .ndo_stop = ethercat_monitor_stop,
    .ndo_start_xmit = ethercat_monitor_tx,
    .ndo_get_stats64 = ethercat_monitor_get_stats64,
};

/**
 * @brief Creates an EtherCAT monitor device
 *
 * @param[in]   monitor_dev    Pointer to EtherCAT monitor device to create.
 * @param[in]   minor          Device minor number.
 * @param[in]   dev_addr       Device MAC address (6-Byte!!)
 * @return 0 on success, -1 on error.
 */
int ethercat_monitor_create(struct monitor_dev *monitor_dev, u16 minor, const unsigned char *dev_addr) {
    int ret = 0, err = 0;
    char monitor_name[64];
    struct ifreq ifr;
    struct net *net;

    monitor_dev->enabled = false; 

    snprintf(&monitor_name[0], 64, "ecat_monitor%d", minor);
    if (!(monitor_dev->net_dev = alloc_netdev(sizeof(struct monitor_dev *), 
                    monitor_name, NET_NAME_UNKNOWN, ether_setup))) 
    {
        pr_err("EtherCAT-Monitor-Device %s: error allocating monitor device\n", monitor_name);
        ret = -1;
    } 
    else
    {
        unsigned char tmp_mac[ETH_ALEN];
        memcpy(&tmp_mac[0], dev_addr, ETH_ALEN);
        tmp_mac[0] = 0x0A; // mark as private

        monitor_dev->net_dev->netdev_ops = &ethercat_monitor_netdev_ops;
        *((struct monitor_dev **)netdev_priv(monitor_dev->net_dev)) = monitor_dev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
        dev_addr_mod(monitor_dev->net_dev, 0, tmp_mac, ETH_ALEN);
#else
        memcpy((void *)monitor_dev->net_dev->dev_addr, tmp_mac, ETH_ALEN);
#endif

        if ((ret = register_netdev(monitor_dev->net_dev))) {
            pr_err("EtherCAT-Monitor-Device %s: error registering monitor net device!\n", monitor_name);
            ret = -1;
        } else {
            netif_carrier_off(monitor_dev->net_dev);
            netif_stop_queue(monitor_dev->net_dev);
        }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
        pr_info("EtherCAT-Monitor-Device %s: created (MAC: %pM)\n", 
                monitor_name, monitor_dev->net_dev->dev_addr);

        // Deactivate interface
        net = dev_net(monitor_dev->net_dev);
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, monitor_name, IFNAMSIZ - 1);
        struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = htonl(0xAC180001 + (minor << 8)); // 172.24.X.1
        err = fcn_devinet_ioctl(net, SIOCSIFADDR, &ifr);
        if (err) {
            pr_err("EtherCAT-Monitor-Device %s: error setting IP address.\n", monitor_name);
        }

        // Set netmask
        struct sockaddr_in *mask = (struct sockaddr_in *)&ifr.ifr_addr;
        mask->sin_family = AF_INET;
        mask->sin_addr.s_addr = htonl(0xFFFFFF00); // 255.255.255.0
        err = fcn_devinet_ioctl(net, SIOCSIFNETMASK, &ifr);
        if (err) {
            pr_err("EtherCAT-Monitor-Device %s: error setting netmask.\n", monitor_name);
        }

        ifr.ifr_flags = monitor_dev->net_dev->flags & ~(IFF_UP | IFF_RUNNING);
        err = fcn_devinet_ioctl(net, SIOCSIFFLAGS, &ifr);
        if (err) {
            pr_err("EtherCAT-Monitor-Device %s: error deactivating interface.\n", monitor_name);
        }

        if (!err) {
            pr_info("EtherCAT-Monitor-Device %s: successfully deactivated monitor device.\n", monitor_name);
        }
#endif
    }

    memset(&monitor_dev->net_dev_stats, 0, sizeof(struct net_device_stats));

    return ret;
}

/** 
 * @brief Destroys an EtherCAT monitor device
 *
 * @param[in]   monitor_dev    Pointer to EtherCAT monitor device to destruct.
 */
void ethercat_monitor_destroy(struct monitor_dev *monitor_dev) {
    if (monitor_dev->net_dev != NULL) {
        unregister_netdev(monitor_dev->net_dev);
        free_netdev(monitor_dev->net_dev);

        monitor_dev->net_dev = NULL;
    }
}

/**
 * @brief Send an EtherCAT frame to the monitor device
 *
 * @param[in]   monitor_dev Pointer to EtherCAT monitor device to destruct.
 * @param[in]   data        Pointer to memory containing the beginning of the EtherCAT frame.
 * @param[in]   datalen     Size of EtherCAT frame.
 */
void ethercat_monitor_frame(struct monitor_dev *monitor_dev, const uint8_t *data, size_t datalen) {
    struct sk_buff *skb = NULL;
    unsigned char *tmp = NULL;

    if (monitor_dev->enabled) {
        skb = __netdev_alloc_skb(monitor_dev->net_dev, ETH_FRAME_LEN, GFP_ATOMIC | __GFP_NOWARN | __GFP_NORETRY);

        if (skb == NULL) {
            monitor_dev->net_dev_stats.rx_dropped++;
        } else {
            tmp = skb_put(skb, datalen);
            memcpy(tmp, data, datalen);

            monitor_dev->net_dev_stats.rx_bytes += datalen;
            monitor_dev->net_dev_stats.rx_packets++;

            skb->dev = monitor_dev->net_dev;
            skb->pkt_type = PACKET_LOOPBACK;
            skb->protocol = eth_type_trans(skb, monitor_dev->net_dev);
            skb->ip_summed = CHECKSUM_UNNECESSARY;

            netif_receive_skb(skb);
        }
    }
}

