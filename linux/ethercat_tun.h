#include <linux/netdevice.h>
#include <linux/cdev.h>
#include <linux/skbuff.h>

// TUN-Device-Structure
struct tun_dev {
    struct net_device *dev;
    struct cdev cdev;
    char name[16];
    u8 mac[6];

    struct sk_buff_head rx_queue;
};

/**
 * @brief Create EtherCAT tun device
 *
 * @param[in]   tun_dev     Pointer to tun_dev structure.
 * @param[in]   minor       tun device minor number.
 */
int ethercat_tun_device_create(struct tun_dev *tun_dev, int minor);

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
