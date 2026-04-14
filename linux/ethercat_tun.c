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

#define DEVICE_NAME "ecat_tun"
#define MAJOR_NUM 240

static struct class *ethercat_tun_class;
static dev_t ethercat_tun_dev_num;

// Prototypen
static int tun_open(struct net_device *dev);
static int tun_stop(struct net_device *dev);
static netdev_tx_t tun_start_xmit(struct sk_buff *skb, struct net_device *dev);
static struct net_device_ops tun_net_ops;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#include <linux/kprobes.h>
static struct kprobe kp = {
    .symbol_name = "kallsyms_lookup_name"
};

typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
kallsyms_lookup_name_t fcn_kallsyms_lookup_name;

typedef int (*fcn_devinet_ioctl_t)(struct net *net, unsigned int cmd, void __user *);
fcn_devinet_ioctl_t fcn_devinet_ioctl;
#endif

// File Operations
static ssize_t ethercat_tun_read(struct file *file, char __user *buf, size_t len, loff_t *off)
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

static ssize_t ethercat_tun_write(struct file *file, const char __user *buf, size_t len, loff_t *off)
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
    skb->protocol = htons(ETH_P_IP); // Beispiel: IP-Paket

    netif_receive_skb(skb);

    return len;
}

static int ethercat_tun_open(struct inode *inode, struct file *file)
{
    struct tun_dev *tun = container_of(inode->i_cdev, struct tun_dev, cdev);
    file->private_data = tun;
    return 0;
}

static int ethercat_tun_release(struct inode *inode, struct file *file)
{
    return 0;
}

static int tun_open(struct net_device *dev)
{
    netif_start_queue(dev);
    printk(KERN_INFO "TUN: Interface %s opened\n", dev->name);
    return 0;
}

static int tun_stop(struct net_device *dev)
{
    netif_stop_queue(dev);
    printk(KERN_INFO "TUN: Interface %s stopped\n", dev->name);
    return 0;
}

static netdev_tx_t tun_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct tun_dev *tun = netdev_priv(dev);

    // Copy packet and queue to rx_queue
    skb = skb_copy(skb, GFP_ATOMIC);
    if (!skb) {
        printk(KERN_ERR "TUN: Out of memory\n");
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

// File Operations
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = ethercat_tun_read,
    .write = ethercat_tun_write,
    .open = ethercat_tun_open,
    .release = ethercat_tun_release,
};

/**
 * @brief Create EtherCAT tun device
 *
 * @param[in]   tun_dev     Pointer to tun_dev structure.
 * @param[in]   minor       tun device minor number.
 */
int ethercat_tun_device_create(struct tun_dev *tun_dev, int minor)
{
    struct net_device *dev;
    struct ifreq ifr;
    struct net *net;
    int err;

    // Setze Namen
    snprintf(tun_dev->name, sizeof(tun_dev->name), DEVICE_NAME "%d", minor);

    // Initialisiere cdev
    cdev_init(&tun_dev->cdev, &fops);
    err = cdev_add(&tun_dev->cdev, ethercat_tun_dev_num, 1);
    if (err)
        goto err_free;

    // Erstelle Device-Datei
    device_create(ethercat_tun_class, NULL, ethercat_tun_dev_num, NULL, DEVICE_NAME);
    dev = alloc_netdev(0, tun_dev->name, NET_NAME_UNKNOWN, ether_setup);
    if (!dev)
        return -ENOMEM;

    tun_dev->dev = dev;
    dev->netdev_ops = &tun_net_ops;
    dev->flags |= IFF_NOARP;
    dev->priv_flags |= IFF_NO_QUEUE;

    // MAC-Adresse setzen
    tun_dev->mac[0] = 0x00;
    tun_dev->mac[1] = 0x11;
    tun_dev->mac[2] = 0x22;
    tun_dev->mac[3] = 0x33;
    tun_dev->mac[4] = 0x44;
    tun_dev->mac[5] = 0x55;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    dev_addr_mod(tun_dev->dev, 0, dev->dev_addr, ETH_ALEN);
#else
    memcpy((void *)dev->dev_addr, tun_dev->mac, ETH_ALEN);
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
    addr->sin_addr.s_addr = htonl(0xC0A86401); // 192.168.100.1
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
    // reserve new major number
    if (alloc_chrdev_region(&ethercat_tun_dev_num, 0, 1, DEVICE_NAME) < 0) {
        printk(KERN_ERR "Fehler bei Device-Nummer\n");
        return -1;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    ethercat_tun_class = class_create(DEVICE_NAME);
#else
    ethercat_tun_class = class_create(THIS_MODULE, DEVICE_NAME);
#endif
    if (IS_ERR(ethercat_tun_class)) {
        unregister_chrdev_region(ethercat_tun_dev_num, 1);
        return PTR_ERR(ethercat_tun_class);
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
	register_kprobe(&kp);
	fcn_kallsyms_lookup_name = (kallsyms_lookup_name_t)kp.addr;
	unregister_kprobe(&kp);

    fcn_devinet_ioctl = (fcn_devinet_ioctl_t)fcn_kallsyms_lookup_name("devinet_ioctl");
#endif

    printk(KERN_INFO "Modul geladen: /dev/%s\n", DEVICE_NAME);

    return 0;
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

