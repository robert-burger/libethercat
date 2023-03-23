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

    // EtherCAT monitor device 
    bool monitor_enabled;                   //! \brief Monitor device enabled.
    struct net_device *monitor_dev;         //! \brief Monitor device net_dev.
    struct net_device_stats monitor_stats;  //! \brief Monitor device statistics.
};

/* ioctls */
#define ETHERCAT_DEVICE_MAGIC             'e'
#define ETHERCAT_DEVICE_MONITOR_ENABLE    _IOW (ETHERCAT_DEVICE_MAGIC, 1, unsigned int)
#define ETHERCAT_DEVICE_GET_POLLING       _IOR (ETHERCAT_DEVICE_MAGIC, 2, unsigned int)

#define ETHERCAT_DEVICE_NET_DEVICE_IOCTL_MAGIC      (0x88A40000)
#define ETHERCAT_DEVICE_NET_DEVICE_DO_POLL          (ETHERCAT_DEVICE_NET_DEVICE_IOCTL_MAGIC | 0x0000)
#define ETHERCAT_DEVICE_NET_DEVICE_GET_POLLING      (ETHERCAT_DEVICE_NET_DEVICE_IOCTL_MAGIC | 0x0001)

//! \brief Module init func.
int ethercat_device_init(void);

int ethercat_device_exit(void);

struct ethercat_device *ethercat_device_create(struct net_device *net_dev);
int ethercat_device_destroy(struct ethercat_device *ecat_dev);
void ethercat_device_receive(struct ethercat_device *ecat_dev, const void *data, size_t size);
void ethercat_device_set_link(struct ethercat_device *ecat_dev, bool link);

#endif 

