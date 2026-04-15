/**
 * \file ethercat_tun.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_tun.h>
#include <linux/if.h>
#include <linux/device.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/inetdevice.h>

#include "ethercat_tun.h"
#include "ethercat_device.h"

#define DEVICE_NAME "ecat_tun"
#define MAJOR_NUM 240

static struct class *ethercat_tun_class;
static dev_t ethercat_tun_dev_num;
u16 ethercat_tun_dev_major;

// forward declarations
static int tun_open(struct net_device *dev);
static int tun_stop(struct net_device *dev);
static netdev_tx_t tun_start_xmit(struct sk_buff *skb, struct net_device *dev);
static struct net_device_ops tun_net_ops;
// File Operations
static ssize_t ethercat_tun_chrdev_read(struct file *file, char __user *buf, size_t len, loff_t *off);
static ssize_t ethercat_tun_chrdev_write(struct file *file, const char __user *buf, size_t len, loff_t *off);
static int ethercat_tun_chrdev_open(struct inode *inode, struct file *file);
static int ethercat_tun_chrdev_release(struct inode *inode, struct file *file);

// File Operations
ssize_t ethercat_tun_chrdev_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
    struct tun_dev *tun = file->private_data;
    struct sk_buff *skb;

    skb = skb_dequeue(&tun->rx_queue);
    if (!skb)
        return 0;

    if (len < skb->len)
        len = skb->len;

    if (copy_to_user(buf, skb->data, len)) {
        kfree_skb(skb);
        return -EFAULT;
    }

    kfree_skb(skb);
    return len;
}

ssize_t ethercat_tun_chrdev_write(struct file *file, const char __user *buf, size_t len, loff_t *off)
{
    struct tun_dev *tun = file->private_data;
    struct sk_buff *skb;

    skb = alloc_skb(len, GFP_KERNEL);
    if (!skb)
        return -ENOMEM;

    if (copy_from_user(skb->data, buf, len)) {
        kfree_skb(skb);
        return -EFAULT;
    }

    skb_put(skb, len);
    skb->dev = tun->dev;
    skb->protocol = htons(ETH_P_IP);

    netif_receive_skb(skb);

    return len;
}

int ethercat_tun_chrdev_open(struct inode *inode, struct file *file)
{
    struct tun_dev *tun = container_of(inode->i_cdev, struct tun_dev, cdev);
    file->private_data = tun;
    return 0;
}

int ethercat_tun_chrdev_release(struct inode *inode, struct file *file)
{
    return 0;
}

// File Operations
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = ethercat_tun_chrdev_read,
    .write = ethercat_tun_chrdev_write,
    .open = ethercat_tun_chrdev_open,
    .release = ethercat_tun_chrdev_release,
};

int tun_open(struct net_device *dev)
{
    netif_start_queue(dev);
    printk(KERN_INFO "EtherCAT-TUN-Device %s: opened\n", dev->name);
    return 0;
}

int tun_stop(struct net_device *dev)
{
    netif_stop_queue(dev);
    printk(KERN_INFO "EtherCAT-TUN-Device %s: stopped\n", dev->name);
    return 0;
}

netdev_tx_t tun_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct tun_dev *tun = *((struct tun_dev **)netdev_priv(dev));

    // Copy packet and queue to rx_queue
    skb = skb_copy(skb, GFP_ATOMIC);
    if (!skb) {
        printk(KERN_ERR "EtherCAT-TUN_Device %s: Out of memory\n", dev->name);
        return NETDEV_TX_OK; 
    }

    skb_queue_tail(&tun->rx_queue, skb);

    // Statistics
    dev->stats.tx_packets++;
    dev->stats.tx_bytes += skb->len;

    return NETDEV_TX_OK;
}

// Netdevice-Operations
static struct net_device_ops tun_net_ops = {
    .ndo_open = tun_open,
    .ndo_stop = tun_stop,
    .ndo_start_xmit = tun_start_xmit,
};

/**
 * @brief Create EtherCAT tun device
 *
 * @param[in]   tun_dev     Pointer to tun_dev structure.
 * @param[in]   minor       tun device minor number.
 * @param[in]   dev_addr    Device MAC address (6-Byte!!)
 * @return 0 on success, -1 on error.
 */
int ethercat_tun_device_create(struct tun_dev *tun_dev, int minor, const unsigned char *dev_addr)
{
    struct net_device *dev;
    struct ifreq ifr;
    struct net *net;
    int err;

    // Setze Namen
    snprintf(tun_dev->name, TUN_DEV_NAME_MAX_LENGTH, DEVICE_NAME "%d", minor);

    pr_info("EtherCAT-Tun-Device %s: creating\n", tun_dev->name);

    // Initialisiere cdev
    cdev_init(&tun_dev->cdev, &fops);
    tun_dev->cdev.owner = THIS_MODULE;
    tun_dev->cdev.ops = &fops;
    err = cdev_add(&tun_dev->cdev, MKDEV(ethercat_tun_dev_major, minor), 1);
    if (err) {
	    pr_err("EtherCAT-Tun-Device %s: error creating character device!\n", tun_dev->name);
	    return err;
    }//        goto err_free;

    // Erstelle Device-Datei
    device_create(ethercat_tun_class, NULL, 
		    MKDEV(ethercat_tun_dev_major, minor), 
		    tun_dev, DEVICE_NAME "%d", minor);

    pr_info("EtherCAT-Tun-Device %s: characted device successfully created.\n", tun_dev->name);

    dev = alloc_netdev(sizeof(struct tun_dev *), tun_dev->name, NET_NAME_UNKNOWN, ether_setup);
    if (!dev) {
	    pr_err("EtherCAT-Tun-Device %s: error creating net device\n", tun_dev->name);
        return -ENOMEM;
    }

    pr_info("EtherCAT-Tun-Device %s: net device successfully created.\n", tun_dev->name);

    tun_dev->dev = dev;
    dev->netdev_ops = &tun_net_ops;
    *((struct tun_dev **)netdev_priv(dev)) = tun_dev;
    dev->flags |= IFF_NOARP;
    dev->priv_flags |= IFF_NO_QUEUE;

    unsigned char tmp_mac[ETH_ALEN];
    memcpy(&tmp_mac[0], dev_addr, ETH_ALEN);
    tmp_mac[0] = 0x0E; // mark as private

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    dev_addr_mod(tun_dev->dev, 0, tmp_mac, ETH_ALEN);
#else
    memcpy((void *)dev->dev_addr, tmp_mac, ETH_ALEN);
#endif

    // Initialize netdev and resources
    skb_queue_head_init(&tun_dev->rx_queue);
    err = register_netdev(dev);
    if (err) {
        free_netdev(dev);
        return err;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    printk(KERN_INFO "EtherCAT-TUN-Device %s created (MAC: %pM)\n", dev->name, dev->dev_addr);

    // Set interface IP address.
    net = dev_net(dev);
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, dev->name, IFNAMSIZ - 1);
    struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = htonl(0xAC190001 + (minor << 8)); // 172.25.X.1
    err = fcn_devinet_ioctl(net, SIOCSIFADDR, &ifr);
    if (err) {
        printk(KERN_ERR "EtherCAT-TUN-Device %s: error setting IP address.\n", dev->name);
        goto err_free;
    }

    // Set netmask
    struct sockaddr_in *mask = (struct sockaddr_in *)&ifr.ifr_addr;
    mask->sin_family = AF_INET;
    mask->sin_addr.s_addr = htonl(0xFFFFFF00); // 255.255.255.0
    err = fcn_devinet_ioctl(net, SIOCSIFNETMASK, &ifr);
    if (err) {
        printk(KERN_ERR "EtherCAT-TUN-Device %s: error setting netmask.\n", dev->name);
        goto err_free;
    }

    // Activate interface
    ifr.ifr_flags = dev->flags | IFF_UP;
    err = fcn_devinet_ioctl(net, SIOCSIFFLAGS, &ifr);
    if (err) {
        printk(KERN_ERR "EtherCAT-TUN-Device %s: error activating interface.\n", dev->name);
        goto err_free;
    }

    printk(KERN_INFO "EtherCAT-TUN-Device %s: IP-Adresse 192.168.100.1/24 set.\n", dev->name);
#endif

    return 0;

err_free:
    unregister_netdev(dev);
    free_netdev(dev);
    return err;
}

/**
 * @brief Destroy EtherCAT tun device
 *
 * @param[in]   tun_dev     Pointer to tun_dev structure.
 */
void ethercat_tun_device_destroy(struct tun_dev *tun_dev) {
    // Entferne cdev
    cdev_del(&tun_dev->cdev);

    // Entferne Netdevice
    unregister_netdev(tun_dev->dev);
    free_netdev(tun_dev->dev);
}


// EtherCAT-TUN-Device initialization
int ethercat_tun_init(void)
{
    int ret = 0;

    pr_info("EtherCAT-Tun-Device: initializing...\n");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    ethercat_tun_class = class_create(DEVICE_NAME);
#else
    ethercat_tun_class = class_create(THIS_MODULE, DEVICE_NAME);
#endif
    if (IS_ERR(ethercat_tun_class)) {
	    pr_err("EtherCAT-Tun-Device: Error during class_create\n");
	    ret = PTR_ERR(ethercat_tun_class);
    }

    // reserve new major number
    if (ret == 0) {
	    if ((ret = alloc_chrdev_region(&ethercat_tun_dev_num, 0, 10, DEVICE_NAME)) < 0) {
		    pr_err("EtherCAT-Tun-Device: Error allocating char device region!\n");
    			class_destroy(ethercat_tun_class);
	    		//unregister_chrdev_region(ethercat_tun_dev_num, 10);
	    }
    }

    ethercat_tun_dev_major = MAJOR(ethercat_tun_dev_num);

    if (ret == 0) {

        pr_info("EtherCAT-Tun-Device: Done initializing.\n");
    }

    return ret;
}

// EtherCAT-TUN-Device cleanup
void ethercat_tun_exit(void)
{
    // Entferne Device-Datei
    device_destroy(ethercat_tun_class, ethercat_tun_dev_num);

    class_destroy(ethercat_tun_class);
    unregister_chrdev_region(ethercat_tun_dev_num, 1);

    printk(KERN_INFO "Modul entladen\n");
}

