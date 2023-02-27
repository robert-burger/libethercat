#ifndef ETHERCAT_DEVICE__H
#define ETHERCAT_DEVICE__H

#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/device.h>
#include <linux/swait.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ioctl.h>

/* spacewire io device structure */
struct ethercat_device {
	int use_as_ethercat_device;

    struct cdev cdev;
    struct device *dev;
    struct spw_dev *spw_dev;
    unsigned minor;
    struct swait_queue_head ir_queue;

    struct net_device *net_dev;

    uint8_t link_state;
    unsigned int poll_mask;

#define EC_TX_RING_SIZE 0x100
    struct sk_buff *tx_skb[EC_TX_RING_SIZE];
    unsigned int tx_skb_index_next;

#define EC_RX_RING_SIZE 0x100
    struct sk_buff *rx_skb[EC_RX_RING_SIZE];

    unsigned int rx_skb_index_last_recv;
    unsigned int rx_skb_index_last_read;

    bool monitor_enabled;
    struct net_device *monitor_dev;
    struct net_device_stats monitor_stats;
};

/* ioctls */
#define ETHERCAT_DEVICE_MAGIC             'e'
#define ETHERCAT_DEVICE_MONITOR_ENABLE    _IOW (ETHERCAT_DEVICE_MAGIC, 1)

int ethercat_device_init(void);
int ethercat_device_exit(void);
struct ethercat_device *ethercat_device_create(struct net_device *net_dev);
int ethercat_device_destroy(struct ethercat_device *ecat_dev);
void ethercat_device_receive(struct ethercat_device *ecat_dev, const void *data, size_t size);
void ethercat_device_set_link(struct ethercat_device *ecat_dev, bool link);

#endif 

