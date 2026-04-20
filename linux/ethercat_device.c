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
#include "ethercat_device_ioctl.h"

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

#include <linux/ethtool.h>

MODULE_AUTHOR("Robert Burger <robert.burger@dlr.de>");
MODULE_DESCRIPTION("ethercat char device driver");
MODULE_LICENSE("GPL");

#define EC_SKB_BUFFERS 256

// forward declarations
int  ethercat_init(void);
void ethercat_exit(void);

static int          ethercat_device_open   (struct inode *inode, struct file *file);
static int          ethercat_device_release(struct inode *inode, struct file *file);
static ssize_t      ethercat_device_read   (struct file *filp, char *buff, size_t len, loff_t *off);
static ssize_t      ethercat_device_write  (struct file *filp, const char *buff, size_t len, loff_t *off);
static long         ethercat_device_unlocked_ioctl(struct file *file, unsigned int num, unsigned long params);
static int          ethercat_device_netdev_do_ioctl(struct ethercat_device *ecat_dev, struct ifreq *ifr, int cmd);
static int          dummy_fcn_devinet_ioctl(struct net *net, unsigned int cmd, void __user *arg);

static struct file_operations ethercat_device_fops = {
    .open           = ethercat_device_open,
    .release        = ethercat_device_release,
    .read           = ethercat_device_read,
    .write          = ethercat_device_write,
    .unlocked_ioctl = ethercat_device_unlocked_ioctl
};

static dev_t ecat_chr_dev;
struct class *ecat_chr_class;
u16 ecat_chr_major = 0;
atomic_t ecat_chr_minor = ATOMIC_INIT(0);
u16 ecat_chr_cnt   = 10;

//#define MODULE_DEBUG
#ifdef MODULE_DEBUG
#define DBG_BUF_SIZE    4096
static char debug_buf[DBG_BUF_SIZE];

#define debug_pr_info(...) pr_info(__VA_ARGS__)
#define debug_printk(...) printk(__VA_ARGS__)
#define debug_print_frame(msg, buf, buflen) {    \
    print_hex_dump_bytes(msg, DUMP_PREFIX_NONE, buf, buflen);
#else
#define debug_pr_info(...)
#define debug_printk(...)
#define debug_print_frame(msg, buf, buflen)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#include <linux/kprobes.h>
static struct kprobe kp = {
    .symbol_name = "kallsyms_lookup_name"
};

typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
kallsyms_lookup_name_t fcn_kallsyms_lookup_name;
#endif

int dummy_fcn_devinet_ioctl(struct net *net, unsigned int cmd, void __user *arg) { return 0; }
fcn_devinet_ioctl_t fcn_devinet_ioctl = &dummy_fcn_devinet_ioctl;

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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    ecat_chr_class = class_create("ecat");
#else
    ecat_chr_class = class_create(THIS_MODULE, "ecat");
#endif
    if ((ret = alloc_chrdev_region(&ecat_chr_dev, 0, ecat_chr_cnt, "ecat")) < 0) {
        pr_info("cannot obtain major nr!\n");
        return ret;
    }

    // get major and minor
    ecat_chr_major = MAJOR(ecat_chr_dev);

    debug_pr_info("allocated major nr: %d\n", ecat_chr_major);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    register_kprobe(&kp);
    fcn_kallsyms_lookup_name = (kallsyms_lookup_name_t)kp.addr;
    unregister_kprobe(&kp);

    fcn_devinet_ioctl_t fcn_tmp = NULL;

    if (fcn_kallsyms_lookup_name) { fcn_tmp = (fcn_devinet_ioctl_t)fcn_kallsyms_lookup_name("devinet_ioctl"); }
    if (fcn_tmp) { 
        pr_info("EtherCAT-Tun-Device: Success getting func pointer for \"devinet_ioctl\".\n");
        fcn_devinet_ioctl = fcn_tmp; 
    } else {
        pr_warn("EtherCAT-Tun-Device: Failed getting func pointer for \"devinet_ioctl\". Setting IP and bringing tun inferface UP not available.\n");
    }
#endif

    ethercat_tun_init();

    return 0;
}

//! \brief EtherCAT device destruction.
/*!
 * \return 0 on success
 */
static int ethercat_device_exit(void) {
    ethercat_tun_exit();

    // unregister the allocated region and character device class
    unregister_chrdev_region(ecat_chr_dev, ecat_chr_cnt);
    class_destroy(ecat_chr_class);

    debug_pr_info("removed.\n");

    return 0;
}

struct ethercat_device *ethercat_device_create(struct net_device *net_dev) {
    struct ethercat_device *ecat_dev;
    int ret = 0;
    unsigned i = 0;

    pr_info("EtherCAT-Char-Device: creating.\n");

    ecat_dev = kmalloc(sizeof(struct ethercat_device), GFP_KERNEL);
    if (!ecat_dev) {
        pr_err("error allocating EtherCAT device\n");
        goto error_exit;
    }

    ecat_dev->net_dev = net_dev;
    ecat_dev->link_state = 0;
    ecat_dev->minor = atomic_fetch_inc(&ecat_chr_minor);

    cdev_init(&ecat_dev->cdev, &ethercat_device_fops);
    ecat_dev->cdev.owner  = THIS_MODULE;
    ecat_dev->cdev.ops    = &ethercat_device_fops;
    if ((ret = cdev_add(&ecat_dev->cdev, MKDEV(ecat_chr_major, ecat_dev->minor), 1)) != 0) {
        pr_err("EtherCAT-Char-Devices ecat%d: error %d adding char devices!", ecat_dev->minor, ret);
        goto error_exit;
    } else {
        // create device node in /dev filesystem
        ecat_dev->dev = device_create(ecat_chr_class, NULL, 
                MKDEV(ecat_chr_major, ecat_dev->minor), ecat_dev, 
                "ecat%d", ecat_dev->minor);
    }

    snprintf(net_dev->name, IFNAMSIZ, "ecat%d", ecat_dev->minor);
    pr_info("EtherCAT-Char-Device %s: created device file.\n", net_dev->name);

    // init skb queues and wait queue
    spin_lock_init(&ecat_dev->queue_lock);
    init_swait_queue_head(&ecat_dev->rx_wait);
    skb_queue_head_init(&ecat_dev->tx_queue);
    skb_queue_head_init(&ecat_dev->rx_queue);
    skb_queue_head_init(&ecat_dev->skb_queue_free);

    for (i = 0; i < EC_SKB_BUFFERS; i++) {
        struct sk_buff *skb = dev_alloc_skb(ETH_FRAME_LEN);
        if (!skb) {
            pr_err("EtherCAT-Char-Device %s: error allocating device socket buffer!\n", net_dev->name);
            goto error_exit;
        }

        skb_queue_tail(&ecat_dev->skb_queue_free, skb);
    }

    ecat_dev->net_dev->netdev_ops->ndo_open(ecat_dev->net_dev);

    (void)ethercat_monitor_create(&ecat_dev->monitor_dev, ecat_dev->minor, net_dev->dev_addr);
    (void)ethercat_tun_device_create(&ecat_dev->tun_dev, ecat_dev->minor, net_dev->dev_addr);

    return ecat_dev;

error_exit:
    if (ecat_dev) {
        while (!skb_queue_empty(&ecat_dev->skb_queue_free)) {
            dev_kfree_skb(skb_dequeue(&ecat_dev->skb_queue_free));
        }

        kfree(ecat_dev);
    }

    return NULL;
}

EXPORT_SYMBOL(ethercat_device_create);

int ethercat_device_destroy(struct ethercat_device *ecat_dev) {
    ethercat_tun_device_destroy(&ecat_dev->tun_dev);
    ethercat_monitor_destroy(&ecat_dev->monitor_dev);

    ecat_dev->net_dev->netdev_ops->ndo_stop(ecat_dev->net_dev);

    while (!skb_queue_empty(&ecat_dev->tx_queue)) {
        dev_kfree_skb(skb_dequeue(&ecat_dev->tx_queue));
    }

    unsigned long flags = 0u;
    spin_lock_irqsave(&ecat_dev->queue_lock, flags);
    while (!skb_queue_empty(&ecat_dev->rx_queue)) {
        dev_kfree_skb(skb_dequeue(&ecat_dev->rx_queue));
    }

    while (!skb_queue_empty(&ecat_dev->skb_queue_free)) {
        dev_kfree_skb(skb_dequeue(&ecat_dev->skb_queue_free));
    }
    spin_unlock_irqrestore(&ecat_dev->queue_lock, flags);

    // char device cleanup
    device_destroy(ecat_chr_class, MKDEV(ecat_chr_major, ecat_dev->minor));
    cdev_del(&ecat_dev->cdev);

    kfree(ecat_dev);
	return 0;
}

EXPORT_SYMBOL(ethercat_device_destroy);

void ethercat_device_set_link(struct ethercat_device *ecat_dev, bool link) {
    if (ecat_dev->link_state != link) {
        if (link) {
            netif_start_queue(ecat_dev->net_dev);
            set_bit(__LINK_STATE_START, &ecat_dev->net_dev->state);
        } else {
            netif_stop_queue(ecat_dev->net_dev);
            clear_bit(__LINK_STATE_START, &ecat_dev->net_dev->state);
        }

        ecat_dev->link_state = link;
        swake_up_one(&ecat_dev->rx_wait);
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
    unsigned long flags = 0u;

    spin_lock_irqsave(&ecat_dev->queue_lock, flags);

    skb = skb_dequeue(&ecat_dev->skb_queue_free); 
    if (!skb && ecat_dev->ethercat_polling) {
        // clean up skb previously sent, maybe this gives us buffers back.
        // unlock spinlock while calling poll_tx
        spin_unlock_irqrestore(&ecat_dev->queue_lock, flags); 

        (void)ethercat_device_netdev_do_ioctl(ecat_dev, NULL, ETHERCAT_DEVICE_NET_DEVICE_DO_POLL_TX);
        
        // relock spinlock and try to get sk_buff
        flags = 0u;
        spin_lock_irqsave(&ecat_dev->queue_lock, flags);
        skb = skb_dequeue(&ecat_dev->skb_queue_free); 
    }
        
    if (!skb) {
        pr_warn("EtherCAT-Char-Device %s: out of buffers!\n", ecat_dev->net_dev->name);
    } else {
        skb->len = size;
        memcpy(skb->data, data, size);
        skb_queue_tail(&ecat_dev->rx_queue, skb);

        debug_print_frame("ethercat char dev driver: received", skb->data, skb->len);

        if (ecat_dev->ethercat_polling == false) {
            ecat_dev->poll_mask |= POLLIN | POLLRDNORM;
            swake_up_one(&ecat_dev->rx_wait);
        }
    }
    
    spin_unlock_irqrestore(&ecat_dev->queue_lock, flags);

    ethercat_monitor_frame(&ecat_dev->monitor_dev, data, size);
}

EXPORT_SYMBOL(ethercat_device_receive);

//! \brief Sent finished function called from network device if a frame was sent.
/*!
 * \param[in]   ecat_dev    Pointer to EtherCAT device.
 * \param[in]   skb         Pointer to socket buffer to free.
 */
void ethercat_device_sent_finished(struct ethercat_device *ecat_dev, struct sk_buff *skb) {
    unsigned long flags = 0u;
    spin_lock_irqsave(&ecat_dev->queue_lock, flags);
    skb->len = 0;
    skb_queue_tail(&ecat_dev->skb_queue_free, skb);
    spin_unlock_irqrestore(&ecat_dev->queue_lock, flags);
}

EXPORT_SYMBOL(ethercat_device_sent_finished);

static int ethercat_device_netdev_do_ioctl(struct ethercat_device *ecat_dev, struct ifreq *ifr, int cmd) {
    int retval = -ENOTTY;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    if (ecat_dev->net_dev->netdev_ops->ndo_eth_ioctl != NULL) { 
        retval = ecat_dev->net_dev->netdev_ops->ndo_eth_ioctl(ecat_dev->net_dev, ifr, cmd);
    } else
#endif
    if (ecat_dev->net_dev->netdev_ops->ndo_do_ioctl != NULL) {
        retval = ecat_dev->net_dev->netdev_ops->ndo_do_ioctl(ecat_dev->net_dev, ifr, cmd);
    }

    return retval;
}

//! driver open function
/*!
 * @param inode pointer to inode structure
 * @param file pointer to file struct
 * @param return 0
 */
static int ethercat_device_open(struct inode *inode, struct file *filp) {
    int local_ret = 0;
    unsigned long flags = 0u;
    struct ethercat_device *ecat_dev;
    ecat_dev = (void *)container_of(inode->i_cdev, struct ethercat_device, cdev);

    debug_pr_info("ethercat char dev driver: open called\n");

    ecat_dev->ethercat_polling = false;

    local_ret = ethercat_device_netdev_do_ioctl(ecat_dev, NULL, ETHERCAT_DEVICE_NET_DEVICE_GET_POLLING);
    if (local_ret > 0) {
        ecat_dev->ethercat_polling = true;
    }

    if (ecat_dev->ethercat_polling) {
        int not_cleaned = 1;
        // consume frames ...
        do {
            not_cleaned = ethercat_device_netdev_do_ioctl(ecat_dev, NULL, ETHERCAT_DEVICE_NET_DEVICE_DO_POLL_RX);
        } while (not_cleaned != 0);
    }
    
    spin_lock_irqsave(&ecat_dev->queue_lock, flags);

    while (!skb_queue_empty(&ecat_dev->rx_queue)) { 
        struct sk_buff *skb = skb_dequeue(&ecat_dev->rx_queue);
        skb->len = 0;
        skb_queue_tail(&ecat_dev->skb_queue_free, skb);
    }
    
    spin_unlock_irqrestore(&ecat_dev->queue_lock, flags);
    
    if (ecat_dev->ethercat_polling) {
        int not_cleaned = 1;
        // clean TX frames ...
        do {
            not_cleaned = ethercat_device_netdev_do_ioctl(ecat_dev, NULL, ETHERCAT_DEVICE_NET_DEVICE_DO_POLL_TX);
        } while (not_cleaned != 0);
    }

    ecat_dev->rx_timeout_ns = 1000000;

    // set user memory to file structure
    filp->private_data = (void*)ecat_dev;

    return 0;
}

//! driver release function
/*!
 * @param inode pointer to inode structure
 * @param file pointer to file struct
 * @param return 0
 */
static int ethercat_device_release(struct inode *inode, struct file *filp) {
    struct ethercat_device *ecat_dev;
    ecat_dev = (struct ethercat_device *)filp->private_data;

    debug_pr_info("libetherat char dev driver: release called\n");

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
    struct sk_buff *skb;
    ssize_t copy_len = 0;
    struct ethercat_device *ecat_dev = (struct ethercat_device *)filp->private_data;
    unsigned long flags = 0u;

    if (!netif_running(ecat_dev->net_dev)) {
        copy_len = -ENETDOWN;
    } else if (!access_ok(buff, len)) {
        copy_len = -EFAULT;
    } else {
        spin_lock_irqsave(&ecat_dev->queue_lock, flags);
        skb = skb_dequeue(&ecat_dev->rx_queue);

        if (skb == NULL) {
            if (ecat_dev->ethercat_polling) {
                copy_len = -ETIMEDOUT;
                s64 start_ns = ktime_get_ns();

                do {
                    // unlock before poll func
                    spin_unlock_irqrestore(&ecat_dev->queue_lock, flags);

                    // controller polling funcs (also cleanup tx buffers)
                    (void)ethercat_device_netdev_do_ioctl(ecat_dev, NULL, ETHERCAT_DEVICE_NET_DEVICE_DO_POLL_TX);
                    (void)ethercat_device_netdev_do_ioctl(ecat_dev, NULL, ETHERCAT_DEVICE_NET_DEVICE_DO_POLL_RX);
                    
                    // lock again
                    flags = 0u;
                    spin_lock_irqsave(&ecat_dev->queue_lock, flags);

                    // check indices
                    skb = skb_dequeue(&ecat_dev->rx_queue);

                    // check timeout
                    if ((ktime_get_ns() - start_ns) >= ecat_dev->rx_timeout_ns) {
                        break;
                    }

                    if (filp->f_flags & O_NONBLOCK) {
                        break;
                    }
                } while (!skb);
            } else { // interrupt mode
                // unlock before poll func
                spin_unlock_irqrestore(&ecat_dev->queue_lock, flags);
                
                int wait_ret = swait_event_interruptible_timeout_exclusive(ecat_dev->rx_wait, !skb_queue_empty(&ecat_dev->rx_queue), HZ);

                // lock again
                flags = 0u;
                spin_lock_irqsave(&ecat_dev->queue_lock, flags);

                if (wait_ret) {
                    skb = skb_dequeue(&ecat_dev->rx_queue);
                }
            }
        }

        if (skb) {
            copy_len = len < skb->len ? len : skb->len;
            if (__copy_to_user(buff, skb->data, copy_len)) {
                copy_len = -EFAULT;
            }

            flags = 0u;
            skb->len = 0;
            skb_queue_tail(&ecat_dev->skb_queue_free, skb);
        }
        

        spin_unlock_irqrestore(&ecat_dev->queue_lock, flags);
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
    struct sk_buff *skb;
    ssize_t ret = 0;
    struct ethercat_device *ecat_dev = (struct ethercat_device *)filp->private_data;
    unsigned long flags = 0u;

    if (!netif_running(ecat_dev->net_dev)) {
        ret = -ENETDOWN;
    } else if (!access_ok(buff, len)) {
        ret = -EFAULT;
    } else if (len > ETH_FRAME_LEN) {
        ret = -EINVAL;
    } else {
        spin_lock_irqsave(&ecat_dev->queue_lock, flags);
        skb = skb_dequeue(&ecat_dev->skb_queue_free);
        spin_unlock_irqrestore(&ecat_dev->queue_lock, flags);
        if (!skb) {
            pr_warn("EtherCAT-Char-Device %s: no more buffer available!\n", ecat_dev->net_dev->name);
            ret = -ENOMEM;
        } else {
            skb->len = len;
            unsigned long bytes_not_copied = __copy_from_user(skb->data, buff, len);
            if (bytes_not_copied != 0) {
                ret = -EINVAL;
            } else {
                netdev_tx_t local_ret = 0;
                debug_print_frame("ethercat char dev driver: sending", skb->data, skb->len);

                skb->dev = ecat_dev->net_dev;
                local_ret = skb->dev->netdev_ops->ndo_start_xmit(skb, skb->dev);
                ethercat_monitor_frame(&ecat_dev->monitor_dev, skb->data, len);
                if (local_ret == NETDEV_TX_OK) {
                    skb = NULL; // will be returned in 'ethercat_device_sent_finished'
                    ret = len;
                } else {
                    ret = -EBUSY;
                }
                
                if (ecat_dev->ethercat_polling) {
                    // clean up skb previously sent, this may not clean up the current skb but from older sends.
                    (void)ethercat_device_netdev_do_ioctl(ecat_dev, NULL, ETHERCAT_DEVICE_NET_DEVICE_DO_POLL_TX);
                }
            }

            if (skb) {
                spin_lock_irqsave(&ecat_dev->queue_lock, flags);
                skb->len = 0;
                skb_queue_tail(&ecat_dev->skb_queue_free, skb);
                spin_unlock_irqrestore(&ecat_dev->queue_lock, flags);
            }
        }
    }

    return ret;
}

static long ethercat_device_unlocked_ioctl(struct file *filp, unsigned int num, unsigned long arg) {
    long ret = 0;
    struct ethercat_device *ecat_dev;

    ecat_dev = (struct ethercat_device *)filp->private_data;

    switch (num) {
        case ETHERCAT_DEVICE_MONITOR_ENABLE: {
            unsigned int enable = 0;
            if (__copy_from_user(&enable, (void *)arg, sizeof(unsigned int))) {
                ret = -EFAULT;
            } else {
                ethercat_monitor_enable(&ecat_dev->monitor_dev, enable);
            }

            break;
        }
        case ETHERCAT_DEVICE_SET_POLLING_RX_TIMEOUT: {
            if (__copy_from_user(&ecat_dev->rx_timeout_ns, (void *)arg, sizeof(uint64_t))) {
                ret = -EFAULT;
            }

            pr_info("set rx_timeout_ns to %lld\n", ecat_dev->rx_timeout_ns);
            break;
        }
        case ETHERCAT_DEVICE_SET_POLLING: { 
            uint32_t set_polling;
            if (__copy_from_user(&set_polling, (void *)arg, sizeof(uint32_t))) {
                ret = -EFAULT;
            }

            if (ret == 0) {
                int local_ret;

                if (set_polling) {
                    (void)ethercat_device_netdev_do_ioctl(ecat_dev, NULL, ETHERCAT_DEVICE_NET_DEVICE_SET_POLLING);
                } else {
                    (void)ethercat_device_netdev_do_ioctl(ecat_dev, NULL, ETHERCAT_DEVICE_NET_DEVICE_RESET_POLLING);
                }

                local_ret = ethercat_device_netdev_do_ioctl(ecat_dev, NULL, ETHERCAT_DEVICE_NET_DEVICE_GET_POLLING);
                if (local_ret > 0) {
                    ecat_dev->ethercat_polling = true;
                } else {
                    ecat_dev->ethercat_polling = false;
                }
            } else {
                ret = -EFAULT;
            }

            break;
        }
        case ETHERCAT_DEVICE_GET_POLLING: {
            uint32_t val = ecat_dev->ethercat_polling == false ? 0 : 1;
            if (__copy_to_user((void *)arg, &val, sizeof(uint32_t))) {
                ret = -EFAULT;
            }
            break;
        }
        case ETHERCAT_DEVICE_GET_LINK_STATE: {
            if (__copy_to_user((void *)arg, &ecat_dev->link_state, sizeof(uint8_t))) {
                ret = -EFAULT;
            }
            break;
        }
        case ETHERCAT_DEVICE_SET_RX_USECS: {
            struct ethtool_coalesce ec;
            struct kernel_ethtool_coalesce kernel_coal;
            struct netlink_ext_ack extack;
            u32 rx_usecs_low = 0u;

            if (__copy_from_user(&rx_usecs_low, (void *)arg, sizeof(uint32_t))) {
                ret = -EFAULT;
            } else {
                ecat_dev->net_dev->ethtool_ops->get_coalesce(
                        ecat_dev->net_dev,
                        &ec, &kernel_coal, &extack);

                ec.rx_coalesce_usecs_low = rx_usecs_low;

                ecat_dev->net_dev->ethtool_ops->set_coalesce(
                        ecat_dev->net_dev,
                        &ec, &kernel_coal, &extack);
            }
            break;
        }
        case ETHERCAT_DEVICE_SET_TX_USECS: {
            struct ethtool_coalesce ec;
            struct kernel_ethtool_coalesce kernel_coal;
            struct netlink_ext_ack extack;
            u32 tx_usecs_low = 0u;

            if (__copy_from_user(&tx_usecs_low, (void *)arg, sizeof(uint32_t))) {
                ret = -EFAULT;
            } else {
                ecat_dev->net_dev->ethtool_ops->get_coalesce(
                        ecat_dev->net_dev,
                        &ec, &kernel_coal, &extack);

                ec.tx_coalesce_usecs_low = tx_usecs_low;

                ecat_dev->net_dev->ethtool_ops->set_coalesce(
                        ecat_dev->net_dev,
                        &ec, &kernel_coal, &extack);
            }
            break;
            break;
        }
        default:
            break;
    }

    return ret;
}

//! initializing ethercat module
/*! 
 */
int  ethercat_init(void) {
    pr_info("EtherCAT-Char-Device: initiliazing\n");

    /* init hardware driver */
    ethercat_device_init();

    return 0;
}

//! exiting ethercat module
/*!
 */
void  ethercat_exit(void) {
    pr_info("EtherCAT-Char-Device: exiting\n");

    ethercat_device_exit();
}

module_init(ethercat_init);
module_exit(ethercat_exit);

