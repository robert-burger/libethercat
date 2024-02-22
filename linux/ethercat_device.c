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

#include "ethercat_device.h"

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

MODULE_AUTHOR("Robert Burger <robert.burger@dlr.de>");
MODULE_DESCRIPTION("libethercat char device driver");
MODULE_LICENSE("GPL");


static int          ethercat_device_open   (struct inode *inode, struct file *file);
static int          ethercat_device_release(struct inode *inode, struct file *file);
static unsigned int ethercat_device_poll   (struct file *file, struct poll_table_struct *poll_table);
static ssize_t      ethercat_device_read   (struct file *filp, char *buff, size_t len, loff_t *off);
static ssize_t      ethercat_device_write  (struct file *filp, const char *buff, size_t len, loff_t *off);
static long         ethercat_device_unlocked_ioctl(struct file *file, unsigned int num, unsigned long params);

static struct file_operations ethercat_device_fops = {
    .open           = ethercat_device_open,
    .release        = ethercat_device_release,
    .poll           = ethercat_device_poll,
    .read           = ethercat_device_read,
    .write          = ethercat_device_write,
    .unlocked_ioctl = ethercat_device_unlocked_ioctl
};

struct ethercat_device_user {
    struct ethercat_device *ecat_dev;
};

static dev_t ecat_chr_dev;
struct class *ecat_chr_class;
u16 ecat_chr_major = 0;
u16 ecat_chr_minor = 0;
u16 ecat_chr_cnt   = 10;

#ifdef MODULE_DEBUG
#define DBG_BUF_SIZE    4096
static char debug_buf[DBG_BUF_SIZE];

#define debug_pr_info(...) pr_info(__VA_ARGS__)
#define debug_printk(...) printk(__VA_ARGS__)
#define debug_print_frame(msg, buf, buflen) {    \
    int debug_print_frame_pos = 0, i;  \
    char *debug_print_frame_buf = &debug_buf[0]; \
    for (i = 0; i < buflen; ++i) {    \
        debug_print_frame_pos += snprintf(debug_print_frame_buf+debug_print_frame_pos, DBG_BUF_SIZE-debug_print_frame_pos, "%02X", buf[i]);  \
    }   \
    pr_info(msg ": %s\n", debug_buf);    \
}
#else
#define debug_pr_info(...)
#define debug_printk(...)
#define debug_print_frame(msg, buf, buflen)
#endif

//================================================================================================
//                                  monitor device stuff
//
// Creates a network interface for monitoring purpose called ecat%d_monitor and registers it to
// the linux network stack. Ensure that the interface is brought up by something like:
//
// $ ip link set up ecat0_monitor
//
// Then use the usual tools to log sent and received EtherCAT frames like tcpdump, wireshark, etc....
//
// WARNING: This should only be enabled for debugging purpose as it may allocate and free memory!!!

//! Open Callback
/*
 * Nothing to open here.
 */
static int ethercat_monitor_open(struct net_device *dev) {
    struct ethercat_device *ecat_dev = *(struct ethercat_device **)netdev_priv(dev);
    ecat_dev->monitor_enabled = true;
    return 0;
}

//! Close Callback
/*
 * Nothing to close here.
 */
static int ethercat_monitor_stop(struct net_device *dev) {
    struct ethercat_device *ecat_dev = *(struct ethercat_device **)netdev_priv(dev);
    ecat_dev->monitor_enabled = false;
    return 0;
}

//! TX callback function
/*
 * Drop all frame someone wants to send to the monitor device from outside.
 */
static int ethercat_monitor_tx(struct sk_buff *skb, struct net_device *dev) {
    struct ethercat_device *ecat_dev = *(struct ethercat_device **)netdev_priv(dev);
    dev_kfree_skb(skb);
    ecat_dev->monitor_stats.tx_dropped++;
    return 0;
}

//! Get statistics callback
static void ethercat_monitor_get_stats64(struct net_device *dev,
			    struct rtnl_link_stats64 *stats) 
{
    struct ethercat_device *ecat_dev = *(struct ethercat_device **)netdev_priv(dev);
    ecat_dev->net_dev->netdev_ops->ndo_get_stats64(ecat_dev->net_dev, stats);
}

//! Network device ops.
static const struct net_device_ops ethercat_monitor_netdev_ops = {
    .ndo_open = ethercat_monitor_open,
    .ndo_stop = ethercat_monitor_stop,
    .ndo_start_xmit = ethercat_monitor_tx,
    .ndo_get_stats64 = ethercat_monitor_get_stats64,
};

//! Creates an EtherCAT monitor device
/*!
 * \param[in]   ecat_dev    Pointer to EtherCAT device to destruct.
 * \return 0 on success, -1 on error.
 */
static int ethercat_monitor_create(struct ethercat_device *ecat_dev) {
    int ret = 0;
    char monitor_name[64];

    ecat_dev->monitor_enabled = false; 

    snprintf(&monitor_name[0], 64, "%s_monitor", ecat_dev->net_dev->name);
    if (!(ecat_dev->monitor_dev = alloc_netdev(sizeof(struct ethercat_device *), 
                    monitor_name, NET_NAME_UNKNOWN, ether_setup))) {
        pr_err("error allocating monitor device\n");
        ret = -1;
    } else {
        ecat_dev->monitor_dev->netdev_ops = &ethercat_monitor_netdev_ops;
        *((struct ethercat_device **)netdev_priv(ecat_dev->monitor_dev)) = ecat_dev;

        memcpy(ecat_dev->monitor_dev->dev_addr, ecat_dev->net_dev->dev_addr, ETH_ALEN);

        if ((ret = register_netdev(ecat_dev->monitor_dev))) {
            pr_err("error registering monitor net device!\n");
            ret = -1;
        }
    }

    memset(&ecat_dev->monitor_stats, 0, sizeof(struct net_device_stats));

    return ret;
}

//! Destroys an EtherCAT monitor device
/*!
 * \param[in]   ecat_dev    Pointer to EtherCAT device to destruct.
 */
static void ethercat_monitor_destroy(struct ethercat_device *ecat_dev) {
    if (ecat_dev->monitor_dev != NULL) {
        unregister_netdev(ecat_dev->monitor_dev);
        free_netdev(ecat_dev->monitor_dev);

        ecat_dev->monitor_dev = NULL;
    }
}

//! Send an EtherCAT frame to the monitor device
/*!
 * \param[in]   ecat_dev    Pointer to EtherCAT device to destruct.
 * \param[in]   data        Pointer to memory containing the beginning of the EtherCAT frame.
 * \param[in]   datalen     Size of EtherCAT frame.
 */
static void ethercat_monitor_frame(struct ethercat_device *ecat_dev, const uint8_t *data, size_t datalen) {
    struct sk_buff *skb = NULL;
    unsigned char *tmp = NULL;

    if (!ecat_dev->monitor_enabled) {
        return;
    }

    skb = netdev_alloc_skb(ecat_dev->monitor_dev, ETH_FRAME_LEN);
    if (skb == NULL) {
        ecat_dev->monitor_stats.rx_dropped++;
        return;
    }

    tmp = skb_put(skb, datalen);
    memcpy(tmp, data, datalen);

    ecat_dev->monitor_stats.rx_bytes += datalen;
    ecat_dev->monitor_stats.rx_packets++;

    skb->dev = ecat_dev->monitor_dev;
    skb->pkt_type = PACKET_LOOPBACK;
    skb->protocol = eth_type_trans(skb, ecat_dev->monitor_dev);
    skb->ip_summed = CHECKSUM_UNNECESSARY;

    netif_rx_ni(skb);
}

//================================================================================================
//                                    ethercat device stuff
//

//! \brief EtherCAT device initialization.
/*!
 * \return 0 on success
 */
static int ethercat_device_init(void) {
	int ret = 0;

	// create driver class and character devices
	ecat_chr_class = class_create(THIS_MODULE, "ecat");
	if ((ret = alloc_chrdev_region(&ecat_chr_dev, 0, ecat_chr_cnt, "ecat")) < 0) {
		pr_info("cannot obtain major nr!\n");
		return ret;
	}

	// get major and minor
	ecat_chr_major = MAJOR(ecat_chr_dev);
	ecat_chr_minor = 0;

	debug_pr_info("allocated major nr: %d\n", ecat_chr_major);

	return 0;
}

//! \brief EtherCAT device destruction.
/*!
 * \return 0 on success
 */
static int ethercat_device_exit(void) {
    // unregister the allocated region and character device class
    unregister_chrdev_region(ecat_chr_dev, ecat_chr_cnt);
    class_destroy(ecat_chr_class);

    debug_pr_info("removed.\n");

	return 0;
}

struct ethercat_device *ethercat_device_create(struct net_device *net_dev) {
    struct ethercat_device *ecat_dev;
    struct ethhdr *eth;
    int ret = 0;
    unsigned i = 0;
    //char monitor_name[64];
    int local_ret;

    debug_pr_info("libethercat: creating EtherCAT character device...\n");

    ecat_dev = kmalloc(sizeof(struct ethercat_device), GFP_KERNEL);
    if (!ecat_dev) {
        pr_err("error allocating EtherCAT device\n");
        goto error_exit;
    }

    ecat_dev->net_dev = net_dev;
    ecat_dev->link_state = 0;
	ecat_dev->minor = ecat_chr_minor++;
    memset(ecat_dev->tx_skb, 0, sizeof(struct sk_buff *) * EC_TX_RING_SIZE);
    ecat_dev->tx_skb_index_next = 0;
    memset(ecat_dev->rx_skb, 0, sizeof(struct sk_buff *) * EC_RX_RING_SIZE);
    ecat_dev->rx_skb_index_last_recv = 0;
    ecat_dev->rx_skb_index_last_read = 0;

    cdev_init(&ecat_dev->cdev, &ethercat_device_fops);
    ecat_dev->cdev.owner  = THIS_MODULE;
    ecat_dev->cdev.ops    = &ethercat_device_fops;
    if ((ret = cdev_add(&ecat_dev->cdev, MKDEV(ecat_chr_major, ecat_dev->minor), 1)) != 0) {
        pr_err("error %d adding ecat%d", ret, ecat_dev->minor);
        goto error_exit;
    } else {
        // create device node in /dev filesystem
        ecat_dev->dev = device_create(ecat_chr_class, NULL, 
                MKDEV(ecat_chr_major, ecat_dev->minor), ecat_dev, 
                "ecat%d", ecat_dev->minor);
    }

    snprintf(net_dev->name, IFNAMSIZ, "ecat%d", ecat_dev->minor);
    debug_pr_info("libethercat: created device file %s.\n", net_dev->name);
    
    // init wait queue
    init_swait_queue_head(&ecat_dev->ir_queue);

    for (i = 0; i < EC_TX_RING_SIZE; i++) {
        struct sk_buff *skb;
        skb = dev_alloc_skb(ETH_FRAME_LEN);
        if (!skb) {
            pr_err("error allocating device socket buffer!\n");
            goto error_exit;
        }

        // add Ethernet-II-header
        skb_reserve(skb, ETH_HLEN);
        eth = (struct ethhdr *) skb_push(skb, ETH_HLEN);
        eth->h_proto = htons(0x88A4);
        memset(eth->h_dest, 0xFF, ETH_ALEN);

        skb->dev = ecat_dev->net_dev;
        memcpy(eth->h_source, ecat_dev->net_dev->dev_addr, ETH_ALEN);

        ecat_dev->tx_skb[i] = skb;
    }
    
    for (i = 0; i < EC_RX_RING_SIZE; i++) {
        if (!(ecat_dev->rx_skb[i] = dev_alloc_skb(ETH_FRAME_LEN))) {
            pr_err("error allocating device socket buffer!\n");
            goto error_exit;
        }
    }

    ecat_dev->net_dev->netdev_ops->ndo_open(ecat_dev->net_dev);

    ecat_dev->ethercat_polling = false;
    local_ret = ecat_dev->net_dev->netdev_ops->ndo_do_ioctl(
            ecat_dev->net_dev, NULL, ETHERCAT_DEVICE_NET_DEVICE_GET_POLLING);
    if (local_ret > 0) {
        ecat_dev->ethercat_polling = true;
    }

    (void)ethercat_monitor_create(ecat_dev);

    return ecat_dev;

error_exit:
    if (ecat_dev) {
        for (i = 0; i < EC_TX_RING_SIZE; i++) {
            if (ecat_dev->tx_skb[i]) {
                dev_kfree_skb(ecat_dev->tx_skb[i]);
            }
        }

        for (i = 0; i < EC_RX_RING_SIZE; i++) {
            if (ecat_dev->rx_skb[i]) {
                dev_kfree_skb(ecat_dev->rx_skb[i]);
            }
        }

        kfree(ecat_dev);
    }

    return NULL;
}

EXPORT_SYMBOL(ethercat_device_create);

int ethercat_device_destroy(struct ethercat_device *ecat_dev) {
    int i = 0;

    ethercat_monitor_destroy(ecat_dev);
    
    ecat_dev->net_dev->netdev_ops->ndo_stop(ecat_dev->net_dev);

    for (i = 0; i < EC_TX_RING_SIZE; i++) {
        if (ecat_dev->tx_skb[i]) {
            dev_kfree_skb(ecat_dev->tx_skb[i]);
        }
    }
    
    for (i = 0; i < EC_RX_RING_SIZE; i++) {
        if (ecat_dev->rx_skb[i]) {
            dev_kfree_skb(ecat_dev->rx_skb[i]);
        }
    }

    kfree(ecat_dev);
	return 0;
}

EXPORT_SYMBOL(ethercat_device_destroy);

void ethercat_device_set_link(struct ethercat_device *ecat_dev, bool link) {
    if (ecat_dev->link_state != link) {
        pr_info("link state changed to %s\n", (link ? "UP" : "DOWN"));
        ecat_dev->link_state = link;

        swake_up_one(&ecat_dev->ir_queue);
    }
}

EXPORT_SYMBOL(ethercat_device_set_link);

//! Packet receive function, called from network driver when a new packet arrives.
/*!
 * @param[in]   ecat_dev    Pointer to ethercat_device.
 * @param[in]   data        Pointer to packet data.
 * @param[in]   size        Size of packet data.
 */
void ethercat_device_receive(struct ethercat_device *ecat_dev, const void *data, size_t size) 
{
    struct sk_buff *skb;
    unsigned int next_index;

    // inc last receive counter
    next_index = (ecat_dev->rx_skb_index_last_recv + 1) % EC_RX_RING_SIZE;
    if (next_index == ecat_dev->rx_skb_index_last_read) {
        // this drops one receive ethercat packet
        ecat_dev->rx_skb_index_last_read = (ecat_dev->rx_skb_index_last_read + 1) % EC_RX_RING_SIZE;
    }

    skb = ecat_dev->rx_skb[next_index];
    memcpy(skb->data, data, size);
    skb->len = size;
    
    debug_print_frame("libethercat char dev driver: received", skb->data, skb->len);

    ecat_dev->rx_skb_index_last_recv = next_index;

    if (ecat_dev->ethercat_polling == false) {
        ecat_dev->poll_mask |= POLLIN | POLLRDNORM;
        swake_up_one(&ecat_dev->ir_queue);
    }

    ethercat_monitor_frame(ecat_dev, data, size);
}

EXPORT_SYMBOL(ethercat_device_receive);

//! driver open function
/*!
 * @param inode pointer to inode structure
 * @param file pointer to file struct
 * @param return 0
 */
static int ethercat_device_open(struct inode *inode, struct file *filp) {
    struct ethercat_device_user *user;
    struct ethercat_device *ecat_dev;
    ecat_dev = (void *)container_of(inode->i_cdev, struct ethercat_device, cdev);

    debug_pr_info("libethercat char dev driver: open called\n");

    if (ecat_dev->ethercat_polling) {
        int not_cleaned = 1;
        // consume frames ...
        do {
            not_cleaned = ecat_dev->net_dev->netdev_ops->ndo_do_ioctl(ecat_dev->net_dev,
                    NULL, ETHERCAT_DEVICE_NET_DEVICE_DO_POLL);
        } while (not_cleaned != 0);
    }
    
    ecat_dev->tx_skb_index_next = 0;
    ecat_dev->rx_skb_index_last_recv = 0;
    ecat_dev->rx_skb_index_last_read = 0;

    // create user memory
    user = kmalloc(sizeof(struct ethercat_device_user), GFP_KERNEL);
    user->ecat_dev = ecat_dev;

    // set user memory to file structure
    filp->private_data = (void*)user;

    return 0;
}

//! driver release function
/*!
 * @param inode pointer to inode structure
 * @param file pointer to file struct
 * @param return 0
 */
static int ethercat_device_release(struct inode *inode, struct file *filp) {
    struct ethercat_device_user *user;
    user = (struct ethercat_device_user *)filp->private_data;
    
    debug_pr_info("libetherat char dev driver: release called\n");

    // free allocated user struct memory
    kfree(user);

    return 0;
}

//! driver read function
/* Called when a process, which already opened the dev file, attempts to
 * read from it.
 * 
 * @param[in]   filp    Pointer to open file.
 * @param[out]  buff    Pointer to user supplied buffer, will be filled with data.
 * @param[in]   len     Length of buffer.
 * @param[in]   off     Offset in buffer.
 * @return read size in bytes or error.
 */
static ssize_t ethercat_device_read(struct file *filp, char *buff, size_t len, loff_t *off)
{
    struct ethercat_device_user *user;
    struct ethercat_device *ecat_dev;
    struct sk_buff *skb;
    size_t copy_len;

    user = (struct ethercat_device_user *)filp->private_data;
    ecat_dev = user->ecat_dev;

    debug_pr_info("libethercat char dev driver: read called\n");

    if (ecat_dev->rx_skb_index_last_recv == ecat_dev->rx_skb_index_last_read) {
        if (filp->f_flags & O_NONBLOCK) {
            // no frame received until now
            return -EWOULDBLOCK; 
        } else {
            if (ecat_dev->ethercat_polling) {
                unsigned long wait_jiffies = jiffies + HZ;

                do {
                    (void)ecat_dev->net_dev->netdev_ops->ndo_do_ioctl(ecat_dev->net_dev, 
                            NULL, ETHERCAT_DEVICE_NET_DEVICE_DO_POLL);

                    if (ecat_dev->rx_skb_index_last_recv != ecat_dev->rx_skb_index_last_read) {
                        break;
                    }
                } while (time_before(jiffies, wait_jiffies));

                if (ecat_dev->rx_skb_index_last_recv == ecat_dev->rx_skb_index_last_read) {
                    return -EAGAIN;
                }
            } else {
                if (!swait_event_interruptible_timeout_exclusive(ecat_dev->ir_queue, 
                            ecat_dev->rx_skb_index_last_recv != ecat_dev->rx_skb_index_last_read, HZ)) {
                    return -EAGAIN;
                }
            }
        }
    }

    ecat_dev->rx_skb_index_last_read = (ecat_dev->rx_skb_index_last_read + 1) % EC_RX_RING_SIZE;
    skb = ecat_dev->rx_skb[ecat_dev->rx_skb_index_last_read];

    copy_len = len < skb->len ? len : skb->len;
    if (__copy_to_user(buff, skb->data, copy_len)) {
        return -EFAULT;
    }

    return copy_len;
}

//! driver read function
/*  Called when a process writes to dev file.
 * 
 * @param[in]   filp    Pointer to open file.
 * @param[in]   buff    Pointer to user supplied buffer.
 * @param[in]   len     Length of buffer.
 * @param[in]   off     Offset in buffer.
 * @return written size in bytes or error.
 */
static ssize_t ethercat_device_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
    struct ethercat_device_user *user;
    struct ethercat_device *ecat_dev;
    struct sk_buff *skb;
    ssize_t ret = 0;

    user = (struct ethercat_device_user *)filp->private_data;
    ecat_dev = user->ecat_dev;
    
    debug_pr_info("libethercat char dev driver: write called\n");
    
    skb = ecat_dev->tx_skb[ecat_dev->tx_skb_index_next++];
    if (ecat_dev->tx_skb_index_next >= EC_TX_RING_SIZE) {
        ecat_dev->tx_skb_index_next = 0;
    }

    skb->len = len;
    if (skb->len > ETH_FRAME_LEN) {
        ret = -EINVAL;
    }
        
    if (ret == 0) {
        /* don't copy ethernet header, use our own */
        unsigned long bytes_not_copied = __copy_from_user(skb->data + ETH_HLEN, buff + ETH_HLEN, len - ETH_HLEN);
        if (bytes_not_copied != 0) {
            ret = -EINVAL;
        } else {
            netdev_tx_t local_ret = 0;
            debug_print_frame("libethercat char dev driver: sending", skb->data, skb->len);
            
            ethercat_monitor_frame(ecat_dev, skb->data, len);

            local_ret = skb->dev->netdev_ops->ndo_start_xmit(skb, skb->dev);
            if (local_ret == NETDEV_TX_OK) {
                ret = len;
            } else {
                ret = -EBUSY;
            }
        }
    }

    return ret;
}

//! driver poll function.
/*! Called by poll and select calls from userlevel.
 *
 * @param[in]   filp        Pointer to open file.
 * @param[in]   poll_table  poll_table_struct struct.
 * @return poll mask
 */
static unsigned int ethercat_device_poll(struct file *filp, struct poll_table_struct *poll_table) {
    struct ethercat_device_user *user;
    struct ethercat_device *ecat_dev;
    unsigned int mask = 0;
    
    user = (struct ethercat_device_user *)filp->private_data;
    ecat_dev = user->ecat_dev;

    debug_pr_info("libethercat char dev driver: poll called\n");

//    poll_wait(filp, &ecat_dev->ir_queue, poll_table);
    mask = ecat_dev->poll_mask;

    if (ecat_dev->link_state) {
        mask |= POLLOUT | POLLWRNORM;
    }

    if (user->ecat_dev->rx_skb_index_last_recv != user->ecat_dev->rx_skb_index_last_read) {
        mask |= POLLIN | POLLRDNORM;
    }

    return mask;
}

static long ethercat_device_unlocked_ioctl(struct file *filp, unsigned int num, unsigned long arg) {
    long ret = 0;
    struct ethercat_device_user *user;
    struct ethercat_device *ecat_dev;
    
    user = (struct ethercat_device_user *)filp->private_data;
    ecat_dev = user->ecat_dev;

    switch (num) {
        case ETHERCAT_DEVICE_GET_POLLING: {
            unsigned int val = ecat_dev->ethercat_polling == false ? 0 : 1;
            if (__copy_to_user((void *)arg, &val, sizeof(unsigned int))) {
                ret = -EFAULT;
            }
            break;
        }
        default:
            break;
    }

    return ret;
}

//! initializing libethercat module
/*! 
 */
int  libethercat_init(void) {
    pr_info("libethercat char dev driver: init\n");

    /* init hardware driver */
    ethercat_device_init();

    return 0;
}

//! exiting libethercat module
/*!
 */
void  libethercat_exit(void) {
    pr_info("libethercat char dev driver: exit\n");

    ethercat_device_exit();
}

module_init(libethercat_init);
module_exit(libethercat_exit);

