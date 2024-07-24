/**
 * \file hw_sock_raw_mmaped_mmaped.c
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief hardware access functions
 *
 */

/*
 * This file is part of libethercat.
 *
 * libethercat is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * libethercat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public 
 * License along with libethercat (LICENSE.LGPL-V3); if not, write 
 * to the Free Software Foundation, Inc., 51 Franklin Street, Fifth 
 * Floor, Boston, MA  02110-1301, USA.
 * 
 * Please note that the use of the EtherCAT technology, the EtherCAT 
 * brand name and the EtherCAT logo is only permitted if the property 
 * rights of Beckhoff Automation GmbH are observed. For further 
 * information please contact Beckhoff Automation GmbH & Co. KG, 
 * Hülshorstweg 20, D-33415 Verl, Germany (www.beckhoff.com) or the 
 * EtherCAT Technology Group, Ostendstraße 196, D-90482 Nuremberg, 
 * Germany (ETG, www.ethercat.org).
 *
 */

#include <libethercat/config.h>
#include <libethercat/hw_sock_raw_mmaped.h>
#include <libethercat/ec.h>
#include <libethercat/idx.h>
#include <libethercat/error_codes.h>

#include <assert.h>

#ifdef LIBETHERCAT_HAVE_NET_IF_H
#include <net/if.h> 
#endif

#ifdef LIBETHERCAT_HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <net/ethernet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <sys/mman.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// forward declarations
int hw_device_sock_raw_mmaped_send(struct hw_common *phw, ec_frame_t *pframe);
int hw_device_sock_raw_mmaped_recv(struct hw_common *phw);
void hw_device_sock_raw_mmaped_send_finished(struct hw_common *phw);
int hw_device_sock_raw_mmaped_get_tx_buffer(struct hw_common *phw, ec_frame_t **ppframe);

// this need the grant_cap_net_raw kernel module 
// see https://gitlab.com/fastflo/open_ethercat
#define GRANT_CAP_NET_RAW_PROCFS "/proc/grant_cap_net_raw"

static int try_grant_cap_net_raw_init(void) {
    int ret = 0;

    if (access(GRANT_CAP_NET_RAW_PROCFS, R_OK) != 0) {
        // does probably not exist or is not readable
    } else {
        int fd = open(GRANT_CAP_NET_RAW_PROCFS, O_RDONLY);
        if (fd == -1) {
            ec_log(1, "HW_OPEN", "error opening %s: %s\n", 
                    GRANT_CAP_NET_RAW_PROCFS, strerror(errno));
            ret = -1;
        } else {
            osal_char_t buffer[1024];
            int n = read(fd, buffer, 1024);
            close(fd);
            if ((n <= 0) || (strncmp(buffer, "OK", 2) != 0)) {
                ret = -1;
            }
        }
    }

    return ret;
}

//! Opens EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   devname     Null-terminated string to EtherCAT hw device name.
 *
 * \return 0 or negative error code
 */
int hw_device_sock_raw_mmaped_open(struct hw_sock_raw_mmaped *phw_sock_raw_mmaped, const osal_char_t *devname) {
    int ret = EC_OK;
    struct ifreq ifr;
    int ifindex;
    
    if (try_grant_cap_net_raw_init() == -1) {
        ec_log(10, "hw_open", "grant_cap_net_raw unsuccessfull, maybe we are "
                "not allowed to open a raw socket\n");
    }
    
    phw_sock_raw_mmaped->common.send = hw_device_sock_raw_mmaped_send;
    phw_sock_raw_mmaped->common.recv = hw_device_sock_raw_mmaped_recv;
    phw_sock_raw_mmaped->common.send_finished = hw_device_sock_raw_mmaped_send_finished;
    phw_sock_raw_mmaped->common.get_tx_buffer = hw_device_sock_raw_mmaped_get_tx_buffer;

    // create raw socket connection
    phw_sock_raw_mmaped->sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ECAT));
    if (phw_sock_raw_mmaped->sockfd <= 0) {
        ec_log(1, "HW_OPEN", "socket error on opening SOCK_RAW: %s\n", strerror(errno));
        ret = EC_ERROR_UNAVAILABLE;
    }

    phw_sock_raw_mmaped->mmap_packets = 100;

    if (ret == EC_OK) {
        int pagesize = getpagesize();
        ec_log(10, "HW_OPEN", "got page size %d bytes\n", pagesize);

        struct tpacket_req tp;

        // tell kernel to export data through mmap()ped ring
        tp.tp_block_size = phw_sock_raw_mmaped->mmap_packets * pagesize;
        tp.tp_block_nr   = 1;
        tp.tp_frame_size = pagesize;
        tp.tp_frame_nr   = phw_sock_raw_mmaped->mmap_packets;
        if (setsockopt(phw_sock_raw_mmaped->sockfd, SOL_PACKET, PACKET_RX_RING, (void*)&tp, sizeof(tp)) != 0) {
            ec_log(1, "HW_OPEN", "setsockopt() rx ring: %s\n", strerror(errno));
            ret = EC_ERROR_UNAVAILABLE;
        } else if (setsockopt(phw_sock_raw_mmaped->sockfd, SOL_PACKET, PACKET_TX_RING, (void*)&tp, sizeof(tp)) != 0) {
            ec_log(1, "HW_OPEN", "setsockopt() tx ring: %s\n", strerror(errno));
            ret = EC_ERROR_UNAVAILABLE;
        } else {}

        if (ret == EC_OK) {
            // TODO unmap anywhere
            phw_sock_raw_mmaped->rx_ring = mmap(0, phw_sock_raw_mmaped->mmap_packets * pagesize * 2, PROT_READ | PROT_WRITE, MAP_SHARED, phw_sock_raw_mmaped->sockfd, 0);
            phw_sock_raw_mmaped->tx_ring = &phw_sock_raw_mmaped->rx_ring[(phw_sock_raw_mmaped->mmap_packets * pagesize)];

            phw_sock_raw_mmaped->rx_ring_offset = 0;
            phw_sock_raw_mmaped->tx_ring_offset = 0;
        }
    }

    if (ret == EC_OK) { 
        int i;

        // set timeouts
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 1;
        setsockopt(phw_sock_raw_mmaped->sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(phw_sock_raw_mmaped->sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        // do not route our frames
        i = 1;
        setsockopt(phw_sock_raw_mmaped->sockfd, SOL_SOCKET, SO_DONTROUTE, &i, sizeof(i));

        // attach to out network interface
        (void)strcpy(ifr.ifr_name, devname);
        ioctl(phw_sock_raw_mmaped->sockfd, SIOCGIFINDEX, &ifr);
        ifindex = ifr.ifr_ifindex;
        (void)strcpy(ifr.ifr_name, devname);
        ifr.ifr_flags = 0;
        ioctl(phw_sock_raw_mmaped->sockfd, SIOCGIFFLAGS, &ifr);

        osal_bool_t iff_running = (ifr.ifr_flags & IFF_RUNNING) == 0 ? OSAL_FALSE : OSAL_TRUE;
        ifr.ifr_flags = ifr.ifr_flags | IFF_PROMISC | IFF_BROADCAST | IFF_UP;
        /*int ret =*/ ioctl(phw_sock_raw_mmaped->sockfd, SIOCSIFFLAGS, &ifr);
        //    if (ret != 0) {
        //        ec_log(1, "HW_OPEN", "error setting interface %s: %s\n", devname, strerror(errno));
        //        goto error_exit;
        //    }
        
        osal_timer_t up_timeout;
        osal_timer_init(&up_timeout, 10000000000);
        while (iff_running == OSAL_FALSE) {
            ioctl(phw_sock_raw_mmaped->sockfd, SIOCGIFFLAGS, &ifr);
            iff_running = (ifr.ifr_flags & IFF_RUNNING) == 0 ? OSAL_FALSE : OSAL_TRUE;
            if (iff_running == OSAL_TRUE) {
                ec_log(10, "HW_OPEN", "interface %s is RUNNING now, wait additional 2 sec for link to be established!\n", devname);
                osal_sleep(1000000000);
            } else {
                ec_log(10, "HW_OPEN", "interface %s is not RUNNING, waiting ...\n", devname);
            }
            
            if (osal_timer_expired(&up_timeout) == OSAL_ERR_TIMEOUT) {
                break;
            }

            osal_sleep(1000000000);
        }

        if (iff_running == OSAL_FALSE) {
            ec_log(1, "HW_OPEN", "unable to bring interface %s UP!\n", devname);
            ret = EC_ERROR_UNAVAILABLE;
            close(phw_sock_raw_mmaped->sockfd);
            phw_sock_raw_mmaped->sockfd = 0;
        }
    }

    if (ret == EC_OK) {
        ec_log(10, "HW_OPEN", "binding raw socket to %s\n", devname);

        (void)memset(&ifr, 0, sizeof(ifr));
        (void)strncpy(ifr.ifr_name, devname, IFNAMSIZ);
        ioctl(phw_sock_raw_mmaped->sockfd, SIOCGIFMTU, &ifr);
        phw_sock_raw_mmaped->common.mtu_size = ifr.ifr_mtu;
        ec_log(10, "hw_open", "got mtu size %d\n", phw_sock_raw_mmaped->common.mtu_size);

        // bind socket to protocol, in this case RAW EtherCAT */
        struct sockaddr_ll sll;
        (void)memset(&sll, 0, sizeof(sll));
        sll.sll_family = AF_PACKET;
        sll.sll_ifindex = ifindex;
        sll.sll_protocol = htons(ETH_P_ECAT);
        bind(phw_sock_raw_mmaped->sockfd, (struct sockaddr *) &sll, sizeof(sll));
    }

    return ret;
}


//! Receive a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 *
 * \return 0 or negative error code
 */
int hw_device_sock_raw_mmaped_recv(struct hw_common *phw) {
    assert(phw != NULL);

    struct hw_sock_raw_mmaped *phw_sock_raw_mmaped = container_of(phw, struct hw_sock_raw_mmaped, common);

    // using kernel mapped receive buffers
    // wait for received, non-processed packet
    struct pollfd pollset;
    pollset.fd = phw_sock_raw_mmaped->sockfd;
    pollset.events = POLLIN;
    pollset.revents = 0;
    int ret = poll(&pollset, 1, 1);
    if (ret > 0) {
        int pagesize = getpagesize();

        struct tpacket_hdr *header;
        for (
                // cppcheck-suppress misra-c2012-11.3
                header = (struct tpacket_hdr *)(&phw_sock_raw_mmaped->rx_ring[(phw_sock_raw_mmaped->rx_ring_offset * pagesize)]);
                header->tp_status & TP_STATUS_USER; 
                // cppcheck-suppress misra-c2012-11.3
                header = (struct tpacket_hdr *)(&phw_sock_raw_mmaped->rx_ring[(phw_sock_raw_mmaped->rx_ring_offset * pagesize)])) 
        {
            // cppcheck-suppress misra-c2012-11.3
            ec_frame_t *real_frame = (ec_frame_t *)(&((osal_char_t *)header)[header->tp_mac]);
            hw_process_rx_frame(phw, real_frame);

            header->tp_status = 0;
            phw_sock_raw_mmaped->rx_ring_offset = (phw_sock_raw_mmaped->rx_ring_offset + 1) % phw_sock_raw_mmaped->mmap_packets;
        }           
    }

    return EC_OK;
}

static struct tpacket_hdr *hw_get_next_tx_buffer(struct hw_common *phw) {
    struct tpacket_hdr *header;
    struct pollfd pollset;

    assert(phw != NULL);
    
    struct hw_sock_raw_mmaped *phw_sock_raw_mmaped = container_of(phw, struct hw_sock_raw_mmaped, common);

    // cppcheck-suppress misra-c2012-11.3
    header = (struct tpacket_hdr *)(&phw_sock_raw_mmaped->tx_ring[(phw_sock_raw_mmaped->tx_ring_offset * getpagesize())]);

    while (header->tp_status != TP_STATUS_AVAILABLE) {
        // notify kernel
        if (send(phw_sock_raw_mmaped->sockfd, NULL, 0, 0) < 0) {
            ec_log(1, "HW_TX", "error on send: %s\n", strerror(errno));
        }

        // buffer not available, wait here...
        pollset.fd = phw_sock_raw_mmaped->sockfd;
        pollset.events = POLLOUT;
        pollset.revents = 0;
        int ret = poll(&pollset, 1, 1000);
        if (ret < 0) {
            ec_log(1, "HW_TX", "error on poll: %s\n", strerror(errno));
            continue;
        }
    }

    return header;
}

//! Get a free tx buffer from underlying hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   ppframe     Pointer to return frame buffer pointer.
 *
 * \return 0 or negative error code
 */
int hw_device_sock_raw_mmaped_get_tx_buffer(struct hw_common *phw, ec_frame_t **ppframe) {
    assert(phw != NULL);
    assert(ppframe != NULL);

    int ret = EC_OK;
    ec_frame_t *pframe = NULL;
    
    static const osal_uint8_t mac_dest[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    static const osal_uint8_t mac_src[] = {0x00, 0x30, 0x64, 0x0f, 0x83, 0x35};

    struct tpacket_hdr *header = NULL;
    header = hw_get_next_tx_buffer(phw);
    // cppcheck-suppress misra-c2012-11.3
    pframe = (ec_frame_t *)(&((osal_char_t *)header)[(TPACKET_HDRLEN - sizeof(struct sockaddr_ll))]);

    // reset length to send new frame
    (void)memcpy(pframe->mac_dest, mac_dest, 6);
    (void)memcpy(pframe->mac_src, mac_src, 6);
    pframe->ethertype = htons(ETH_P_ECAT);
    pframe->type = 0x01;
    pframe->len = sizeof(ec_frame_t);

    *ppframe = pframe;

    return ret;
}

//! Send a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   pframe      Pointer to frame buffer.
 *
 * \return 0 or negative error code
 */
int hw_device_sock_raw_mmaped_send(struct hw_common *phw, ec_frame_t *pframe) {
    assert(phw != NULL);
    assert(pframe != NULL);

    int ret = EC_OK;
    struct hw_sock_raw_mmaped *phw_sock_raw_mmaped = container_of(phw, struct hw_sock_raw_mmaped, common);

    // fill header
    struct tpacket_hdr *header = NULL;
    header = (struct tpacket_hdr *)(((osal_char_t *)pframe) - sizeof(struct tpacket_hdr));
    header->tp_len = pframe->len;
    header->tp_status = TP_STATUS_SEND_REQUEST;

    // notify kernel
    if (send(phw_sock_raw_mmaped->sockfd, NULL, 0, 0) < 0) {
        ec_log(1, "HW_TX", "error on sendto: %s\n", strerror(errno));
        ret = EC_ERROR_HW_SEND;
    }

    // increase consumer ring pointer
    phw_sock_raw_mmaped->tx_ring_offset = (phw_sock_raw_mmaped->tx_ring_offset + 1) % phw_sock_raw_mmaped->mmap_packets;

    return ret;
}

//! Doing internal stuff when finished sending frames
/*!
 * \param[in]   phw         Pointer to hw handle.
 */
void hw_device_sock_raw_mmaped_send_finished(struct hw_common *phw) {
}


