//! Simple virtual ethernet switch
/*!
 * author: Robert Burger
 *
 * $Id$
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

#ifdef HAVE_CONFIG_H
#include <libethercat/config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef LIBETHERCAT_HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef LIBETHERCAT_HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif

#ifdef LIBETHERCAT_HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#ifdef LIBETHERCAT_HAVE_NETINET_UDP_H
#include <netinet/udp.h>
#endif

#ifdef LIBETHERCAT_HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <errno.h>

#include "libethercat/veth.h"
#include "libethercat/ec.h"
#include "libethercat/error_codes.h"
#include "libethercat/settings.h"
#include "libethercat/coe.h"
#include "libethercat/mbx_gateway.h"

#ifdef LIBETHERCAT_HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef LIBETHERCAT_HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef LIBETHERCAT_HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#if LIBETHERCAT_BUILD_POSIX
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <netinet/ip_icmp.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>

#include <linux/if.h>
#include <linux/if_tun.h>
#endif

// forward declarations
static void calculate_icmp_checksum(struct icmphdr *icmp, size_t len);
static void calculate_udp_checksum(struct iphdr *ip, struct udphdr *udp);
static void calculate_ip_checksum(struct iphdr *ip);
static void setup_ether_header(struct ether_header *eth_hdr, const uint8_t shost[ETH_ALEN], 
        const uint8_t dhost[ETH_ALEN], const uint16_t ether_type);
static void setup_iphdr(struct iphdr* ip, uint8_t protocol, uint16_t len, uint32_t src, uint32_t dst);
static void setup_udphdr(struct udphdr* udp, uint16_t sport, uint16_t dport, uint16_t len);

/**
 * @brief Fill in Ethernet header with values.
 *
 * @param[out]  eth_hdr     Pointer to Ethernet header.
 * @param[in]   shost       Source MAC address to be set.
 * @param[in]   dhost       Destination MAC address to be set.
 * @param[in]   ether_type  EtherType to be set.
 */
static void setup_ether_header(struct ether_header *eth_hdr, const uint8_t shost[ETH_ALEN], 
        const uint8_t dhost[ETH_ALEN], const uint16_t ether_type) 
{
    memcpy(eth_hdr->ether_dhost, &dhost[0], ETH_ALEN);
    memcpy(eth_hdr->ether_shost, &shost[0], ETH_ALEN);
    eth_hdr->ether_type = htons(ether_type);
}

/**
 * @brief Fill in IP header with values.
 *
 * @param[out]  ip          Pointer to IP header.
 * @param[in]   protocol    IP protocol to be set.
 * @param[in]   len         Complete length of IP packet.
 * @param[in]   src         Source IP address to be set.
 * @param[in]   dst         Destination IP address to be set.
 */
static void setup_iphdr(struct iphdr* ip, uint8_t protocol, uint16_t len, uint32_t src, uint32_t dst) {
    ip->version = 4;
    ip->ihl = 5;
    ip->tos = 0;
    ip->tot_len = htons(len);
    ip->id = htons(12345);
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->saddr = src;
    ip->daddr = dst;
    ip->check = 0; // wird später berechnet
}

/**
 * @brief Fill in UDP header with values.
 *
 * @param[out]  udp         Pointer to UDP header.
 * @param[in]   sport       Source port number to be set.
 * @param[in]   dport       Destination port number to be set.
 * @param[in]   len         Length of UDP packet.
 */
static void setup_udphdr(struct udphdr* udp, uint16_t sport, uint16_t dport, uint16_t len) {
    udp->uh_sport = htons(sport);
    udp->uh_dport = htons(dport);
    udp->uh_ulen = htons(len);
    udp->uh_sum = 0; // für UDP ohne Checksum: 0
}

// Define HIWORD and LOWORD macros
#define LOWORD(l) ((uint16_t)(l))
#define HIWORD(l) ((uint16_t)((l) >> 16))

/**
 * @brief Calculate ICMP checksum
 *
 * @param[in]   data            Pointer to start of ICMP-header.
 * @param[in]   len             Lenght of ICMP-header and payload.
 */
static void calculate_icmp_checksum(struct icmphdr *icmp, size_t len) {
    uint32_t sum = 0;
    const uint16_t *data = (const uint16_t *)icmp;

    // add the udp packet (hdr + data), initialize checksum to 0
    icmp->checksum = 0;
    while (len > 1) {
        sum += * data++;
        len -= 2;
    }

    // pad if udp_len was uneven
    if (len > 0) {
        sum += ((*data) & htons(0xFF00));
    }

    // fold sum to 16 bits: add carrier to result
    while (sum >> 16) {
        sum = LOWORD(sum) + HIWORD(sum);
    }

    // 1's complement
    sum = ~sum;
    icmp->checksum = sum;
}

/**
 * @brief Calculate UDP checksum
 * 
 * @param[in]       ip      Pointer to IP header.
 * @param[in,out]   udp     Pointer to UDP datagram header.
 */
static void calculate_udp_checksum(struct iphdr *ip, struct udphdr *udp) {
    register unsigned long sum = 0;
    const uint16_t *data = (const uint16_t *)udp;
    uint16_t udp_len = htons(udp->len);

    // construct pseudo header 
    sum += HIWORD(ip->saddr) + LOWORD(ip->saddr);
    sum += HIWORD(ip->daddr) + LOWORD(ip->daddr);
    sum += htons(IPPROTO_UDP);
    sum += udp->len;

    // add the udp packet (hdr + data), initialize checksum to 0
    udp->check = 0;
    while (udp_len > 1) {
        sum += * data++;
        udp_len -= 2;
    }

    // pad if udp_len was uneven
    if (udp_len > 0) {
        sum += ((*data) & htons(0xFF00));
    }

    // fold sum to 16 bits: add carrier to result
    while (sum >> 16) {
        sum = LOWORD(sum) + HIWORD(sum);
    }

    // 1's complement
    sum = ~sum;
    udp->check = ((uint16_t)sum == 0x0000) ? 0xFFFF : (uint16_t)sum;
}

/**
 * @brief Calculate TCP checksum
 * 
 * @param[in]       ip      Pointer to IP header.
 */
static void calculate_ip_checksum(struct iphdr *ip) {
    uint32_t sum = 0;
    const uint16_t* data = (const uint16_t *)ip;
    uint16_t len = sizeof(struct iphdr);

    // add the tcp header, initialize checksum to 0
    ip->check = 0;
    while (len > 1) {
        sum += *data++;
        len -= 2;
    }

    // iphdr struct should be even number of byte, but just to have it here
    if (len) {
        sum += *(uint8_t*)data;
    }

    // fold sum
    sum = HIWORD(sum) + LOWORD(sum);
    sum += HIWORD(sum);

    // 1's complement
    sum = ~sum;
    ip->check = sum;
}

static inline void veth_debug_print(ec_t *pec, const osal_char_t *ctx, const osal_char_t *msg, osal_uint8_t *frame, osal_size_t frame_len) {
#define EOE_DEBUG_BUFFER_SIZE   1024
    static osal_char_t eoe_debug_buffer[EOE_DEBUG_BUFFER_SIZE];
    
    int pos = snprintf(eoe_debug_buffer, EOE_DEBUG_BUFFER_SIZE, "%s: ", msg);
    for (osal_uint32_t i = 0; (i < frame_len) && (pos < EOE_DEBUG_BUFFER_SIZE); ++i) {
        pos += snprintf(&eoe_debug_buffer[pos], EOE_DEBUG_BUFFER_SIZE-pos, "%02X", frame[i]);
    }

    ec_log(10, ctx, "%s\n", eoe_debug_buffer);
}

/**
 * @brief Process received Ethernet frame.
 *
 * @param[in]   pec         Pointer to EtherCAT Master structure.
 * @param[in]   buf         Buffer pointing to Ethernet frame.
 * @param[in]   len         Length of Ethernet frame.
 */
void ec_veth_process_frame(struct ec *pec, uint8_t *buf, size_t len) {
    struct ether_header *eth_hdr = (struct ether_header *)buf;

    switch (ntohs(eth_hdr->ether_type)) {
        case ETHERTYPE_ARP: {
            struct arppayload {
                uint8_t sender_mac[6];
                uint8_t sender_ip[4];
                uint8_t target_mac[6];
                uint8_t target_ip[4];
            }; 

            struct arphdr *arp_hdr = (struct arphdr *)(buf + sizeof(struct ether_header));
            struct arppayload *arp_payload = (struct arppayload *)((uint8_t *)arp_hdr + sizeof(struct arphdr));

            if (memcmp(&arp_payload->target_ip[0], (uint8_t*)&pec->veth.ip, 4) == 0) {
                // this is an ARP request to our master IP address, answer it!
                memcpy(arp_payload->target_mac, arp_payload->sender_mac, ETH_ALEN);
                memcpy(arp_payload->sender_mac, pec->veth.mac, ETH_ALEN);
                memcpy(&arp_payload->target_ip[0], &arp_payload->sender_ip[0], 4);
                memcpy(&arp_payload->sender_ip[0], (uint8_t*)&pec->veth.ip, 4);
                arp_hdr->ar_op = htons(ARPOP_REPLY);

                // exchange eth MAC
                setup_ether_header(eth_hdr, eth_hdr->ether_dhost, eth_hdr->ether_shost, ETHERTYPE_ARP);

                int wr = ec_veth_send_frame(pec, buf, len);
                (void)wr;
            }
            break;
        }
        case ETHERTYPE_IP: {
            struct iphdr* ip = (struct iphdr*)(buf + sizeof(struct ether_header));
            if (ip->protocol == IPPROTO_ICMP) {
                struct icmphdr *icmphdr = (struct icmphdr *)((uint8_t *)ip + sizeof(struct iphdr));
                icmphdr->type = ICMP_ECHOREPLY;
                calculate_icmp_checksum(icmphdr, ntohs(ip->tot_len) - sizeof(struct iphdr));

                // setup ether_header and ip header
                setup_ether_header(eth_hdr, eth_hdr->ether_dhost, eth_hdr->ether_shost, ETHERTYPE_IP);
                setup_iphdr(ip, IPPROTO_ICMP, htons(ip->tot_len), ip->daddr, ip->saddr);
                calculate_ip_checksum(ip);

                int wr = ec_veth_send_frame(pec, buf, len);
                (void)wr;
            } else if (ip->protocol == IPPROTO_TCP) {
                ec_log(1, "VETH", "received TCP request, not implemented!");
            } else if (ip->protocol == IPPROTO_UDP) {
                struct udphdr* udp = (struct udphdr*)((uint8_t *)ip + sizeof(struct iphdr));
                uint16_t src_port = ntohs(udp->uh_sport);
                uint16_t dst_port = ntohs(udp->uh_dport);

                if (dst_port == 0x88A4) {
                    struct echdr *echdr = (struct echdr *)((uint8_t *)udp + sizeof(struct udphdr));
                    if (ec_mbx_gateway_handle(pec, echdr, len) == EC_OK) {
                        // setup headers
                        setup_ether_header(eth_hdr, eth_hdr->ether_dhost, eth_hdr->ether_shost, ETHERTYPE_IP);
                        setup_udphdr(udp, dst_port, src_port, ntohs(udp->len));
                        setup_iphdr(ip, IPPROTO_UDP, htons(ip->tot_len), ip->daddr, ip->saddr);

                        ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + 2 + echdr->length);
                        udp->len = htons(sizeof(struct udphdr) + 2 + echdr->length);

                        calculate_ip_checksum(ip);
                        calculate_udp_checksum(ip, udp);

                        size_t send_length = htons(ip->tot_len) + sizeof(struct ethhdr);
                        int wr = ec_veth_send_frame(pec, buf, send_length);
                        if (wr != send_length) {
                            ec_log(1, "VETH", "tried to send back %lu bytes, got %d through\n", send_length, wr);
                        }
                    }
                }
            }

            break;
        }
    }
}

/**
 * @brief Send Ethernet frame over tun.
 *
 * @param[in]   pec         Pointer to EtherCAT Master structure.
 * @param[in]   buf         Buffer pointing to Ethernet frame.
 * @param[in]   len         Length of Ethernet frame.
 */
int ec_veth_send_frame(struct ec *pec, uint8_t *buf, size_t len) {
    return write(pec->veth.fd, buf, len);
}

//! \brief Handler thread for tap interface
/*!
 * \param[in] pec           Pointer to ethercat master structure, 
 *                          which you got from \link ec_open \endlink.
 */
static void ec_veth_tun_handler(ec_t *pec) {
    assert(pec != NULL);

    eth_frame_t tmp_frame;

    ec_log(10, "VETH", "tun handler started\n");

    while (pec->veth.running == OSAL_TRUE) {
        int ret;
        fd_set rd_set;

        FD_ZERO(&rd_set);
        FD_SET(pec->veth.fd, &rd_set);

        struct timeval tv = {0, 100000};   // sleep for 100 ms
        ret = select(pec->veth.fd + 1, &rd_set, NULL, NULL, &tv);
        int local_errno = errno;

        if ((ret < 0) && (local_errno == EINTR)){
            (void)printf("select returned %d, errno %d\n", ret, errno);
            continue;
        }

        if (ret < 0) {
            perror("select()");
        } else {
            if (FD_ISSET(pec->veth.fd, &rd_set) != 0) {
                int rd = read(pec->veth.fd, &tmp_frame.frame_data[0], sizeof(tmp_frame.frame_data));
                if (rd > 0) {
                    // simple switch here 
                    struct ether_header *eth_hdr = (struct ether_header *)&tmp_frame.frame_data[0];
                    static const osal_uint8_t broadcast_mac[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
                    int is_broadcast = 0;
                    if (memcmp(broadcast_mac, eth_hdr->ether_dhost, ETH_ALEN) == 0) {
                        is_broadcast = 1;

                        ec_veth_process_frame(pec, &tmp_frame.frame_data[0], rd);
                    } else if (memcmp(eth_hdr->ether_dhost, pec->veth.mac, ETH_ALEN) == 0) {
                        // something directly for the master
                        ec_veth_process_frame(pec, &tmp_frame.frame_data[0], rd);
                    }

                    for (osal_uint16_t slave = 0; slave < pec->slave_cnt; ++slave) {
                        ec_slave_ptr(slv, pec, slave);
                        if (    !slv->eoe.use_eoe ||
                                (slv->act_state == EC_STATE_INIT)   ||
                                (slv->act_state == EC_STATE_BOOT)) {
                            continue;
                        }

                        if (is_broadcast || (memcmp(slv->eoe.mac, eth_hdr->ether_dhost, 6) == 0)) {
                            ec_log(100, "VETH", "slave %2d: sending eoe frame\n", slave);
                            (void)ec_eoe_send_frame(pec, slave, tmp_frame.frame_data, rd);

                            if (!is_broadcast) {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    ec_log(10, "VETH", "tun handler stopped\n");
}

void *ec_veth_tun_handler_wrapper(void *arg) {
    // cppcheck-suppress misra-c2012-11.5
    ec_t *pec = arg;
    ec_veth_tun_handler(pec);
    return NULL;
}

/**
 * @brief Open exisiting TUN device like '/dev/ecat_tun0' created by ethercat_chardev.ko!
 *
 * @param[in]   pec             Pointer to ethercat master structure.
 * @param[in]   tun_dev_name    Name of tun character device file.
 * @param[in]   mac_addr        MAC address of master device.
 * @param[in]   ip_addr         IP address of master device.
 * @retval EC_OK                Opening and starting of tun device successfull
 * @retval EC_ERROR_UNAVAILABLE Tun device file could not be opened.
 */
int ec_veth_open_tun(struct ec *pec, const char *tun_dev_name, const uint8_t mac_addr[ETH_ALEN], const uint32_t ip_addr) {
    assert(pec != NULL);

    int ret = EC_OK;

    if (strncmp(tun_dev_name, "file:", 5) == 0) {
        tun_dev_name = &tun_dev_name[5];
        memcpy(pec->veth.mac, mac_addr, 6);
        pec->veth.ip = ip_addr;
        pec->veth.fd = open(tun_dev_name, O_RDWR, 0644);
        if (pec->veth.fd == -1) {
            ec_log(1, "EOE_OPEN_TUN", "could not open %s\n", tun_dev_name);
            ret = EC_ERROR_UNAVAILABLE;
        } else {
            ec_log(10, "EOE_OPEN_TUN", "tun device %s successfully opened, master ip is %X!\n", tun_dev_name, pec->veth.ip);
        }

        if (ret == EC_OK) {
            pec->veth.running = OSAL_TRUE;
            osal_task_attr_t attr;
            attr.policy = OSAL_SCHED_POLICY_FIFO;
            attr.priority = 5;
            attr.affinity = 0xFF;
            (void)strcpy(&attr.task_name[0], "ecat.tun");
            osal_task_create(&pec->veth.tid, &attr, ec_veth_tun_handler_wrapper, pec);
        }
    } else { // create tun
#if LIBETHERCAT_BUILD_POSIX == 1
        int s;
        struct ifreq ifr;
        (void)memset(&ifr, 0, sizeof(ifr));

        // open tun device
        pec->veth.fd = open("/dev/net/tun", O_RDWR);
        if (pec->veth.fd == -1) {
            ec_log(1, "EOE_SETUP_TUN", "could not open /dev/net/tun\n");
            ret = EC_ERROR_UNAVAILABLE;
        } 

        if (ret == EC_OK) {
            ifr.ifr_flags = IFF_TAP | IFF_NO_PI; 

            if (ioctl(pec->veth.fd, TUNSETIFF, (void *)&ifr) != 0) {
                ec_log(1, "EOE_SETUP_TUN", "could not request tun/tap device\n");
                close(pec->veth.fd);
                pec->veth.fd = 0;
                ret = EC_ERROR_UNAVAILABLE;
            } 
        }

        if (ret == EC_OK) {
            ec_log(10, "EOE_SETUP_TUN", "using interface %s\n", ifr.ifr_name);

            // Create a socket
            s = socket(AF_INET, SOCK_DGRAM, 0);
            if (s < 0) {
                perror("socket");
                ret = EC_ERROR_UNAVAILABLE;
            } 
        }

        if (ret == EC_OK) {
            if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0) { // Get interface flags
                perror("cannot get interface flags");
                ret = EC_ERROR_UNAVAILABLE;
            } 
        }

        if (ret == EC_OK) {
            // Turn on interface
            ifr.ifr_flags |= IFF_UP;
            if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0) {
                (void)fprintf(stderr, "ifup: failed ");
                perror(ifr.ifr_name);
                ret = EC_ERROR_UNAVAILABLE;
            } 
        }

        if (ret == EC_OK) {
            // Set interface address
            struct sockaddr_in  my_addr;
            bzero((osal_char_t *) &my_addr, sizeof(my_addr));
            my_addr.sin_family = AF_INET;
            my_addr.sin_addr.s_addr = htonl(pec->veth.ip);
            (void)memcpy(&ifr.ifr_addr, &my_addr, sizeof(struct sockaddr));

            if (ioctl(s, SIOCSIFADDR, &ifr) < 0) {
                (void)fprintf(stderr, "Cannot set IP address. ");
                perror(ifr.ifr_name);
                ret = EC_ERROR_UNAVAILABLE;
            } else {
                pec->veth.running = OSAL_TRUE;
                osal_task_attr_t attr;
                attr.policy = OSAL_SCHED_POLICY_FIFO;
                attr.priority = 5;
                attr.affinity = 0xFF;
                (void)strcpy(&attr.task_name[0], "ecat.tun");
                osal_task_create(&pec->veth.tid, &attr, ec_veth_tun_handler_wrapper, pec);
            }
        }
#else 
        (void)pec;
#endif
    }
    
    (void)pool_open(&pec->veth.recv_pool, 0, NULL);

    return ret;
}

/**
 * @brief Close previously opened TUN device.
 *
 * @param[in]   pec             Pointer to ethercat master structure.
 */
void ec_veth_close_tun(struct ec *pec) {
    assert(pec != NULL);

    if (pec->veth.running == OSAL_TRUE) {
        pec->veth.running = OSAL_FALSE;
#if LIBETHERCAT_BUILD_POSIX == 1
        osal_task_join(&pec->veth.tid, NULL);

        close(pec->veth.fd);
        pec->veth.fd = 0;
#endif
    }
    
    (void)pool_close(&pec->veth.recv_pool);
}

