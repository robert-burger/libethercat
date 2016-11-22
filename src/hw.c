//! ethercat hardware layer
/*!
 * author: Robert Burger
 *
 * $Id$
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

#define _GNU_SOURCE
#include <sched.h>

#include "libethercat/hw.h"
#include "libethercat/ec.h"
#include "libethercat/idx.h"

#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#ifdef __linux__
#include <netpacket/packet.h>
#elif defined __VXWORKS__
#include <vxWorks.h>
#include <taskLib.h>
#else
#error unsupported OS
#endif
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define ETH_P_ECAT      0x88A4
#define ETH_FRAME_LEN   1518 

//! receiver thread forward declaration
void *hw_rx_thread(void *arg);

//! open a new hw
/*!
 * \param pphw return hw 
 * \param devname ethernet device name
 * \param prio receive thread prio
 * \param cpumask receive thread cpumask
 * \return 0 or negative error code
 */
int hw_open(hw_t **pphw, const char *devname, int prio, int cpumask) {
#ifdef __linux__
    struct timeval timeout;
    int i, ifindex;
    struct ifreq ifr;
    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));

    ec_log(10, __func__, "size sockaddr %d, sockaddr_ll %d\n", 
            sizeof(struct sockaddr), sizeof(struct sockaddr_ll));
    printf("size sockaddr %lu, sockaddr_ll %lu\n", 
            sizeof(struct sockaddr), sizeof(struct sockaddr_ll));
#endif

    (*pphw) = (hw_t *)malloc(sizeof(hw_t));
    if (!(*pphw))
        return -ENOMEM;

    memset(*pphw, 0, sizeof(hw_t));

    datagram_pool_open(&(*pphw)->tx_high, 0);
    datagram_pool_open(&(*pphw)->tx_low, 0);
    
#ifdef __linux__
    // create raw socket connection
    (*pphw)->sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ECAT));
    if ((*pphw)->sockfd <= 0) {
        perror("socket error on opening SOCK_RAW");
        goto error_exit;
    }
   
    // set timeouts
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000; 
    setsockopt((*pphw)->sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt((*pphw)->sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // do not route our frames
    i = 1;
    setsockopt((*pphw)->sockfd, SOL_SOCKET, SO_DONTROUTE, &i, sizeof(i));

    // attach to out network interface
    strcpy(ifr.ifr_name, devname);
    ioctl((*pphw)->sockfd, SIOCGIFINDEX, &ifr);
    ifindex = ifr.ifr_ifindex;
    strcpy(ifr.ifr_name, devname);
    ifr.ifr_flags = 0;
    ioctl((*pphw)->sockfd, SIOCGIFFLAGS, &ifr);
    ifr.ifr_flags = ifr.ifr_flags | IFF_PROMISC | IFF_BROADCAST | IFF_UP;
    ioctl((*pphw)->sockfd, SIOCSIFFLAGS, &ifr);
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, devname, sizeof(ifr.ifr_name));
    ioctl((*pphw)->sockfd, SIOCGIFMTU, &ifr);
    (*pphw)->mtu_size   = ifr.ifr_mtu;
    ec_log(10, "hw_open", "got mtu size %d\n", (*pphw)->mtu_size);

    // bind socket to protocol, in this case RAW EtherCAT */
    sll.sll_family      = AF_PACKET;
    sll.sll_ifindex     = ifindex;
    sll.sll_protocol    = htons(ETH_P_ECAT);
    bind((*pphw)->sockfd, (struct sockaddr *)&sll, sizeof(sll));
#elif defined __VXWORKS__
    /* we use snarf link layer device driver */
    (*pphw)->sockfd = open(devname, O_RDWR, 0644);
    if ((*pphw)->sockfd <= 0) {
        perror("open");
        goto error_exit;
    }

    (*pphw)->mtu_size = 1480;
#else
#error unsopported OS
#endif

    // thread settings
    (*pphw)->rxthreadprio = prio;
    (*pphw)->rxthreadcpumask = cpumask;
    (*pphw)->rxthreadrunning = 1;
    pthread_create(&(*pphw)->rxthread, NULL, hw_rx_thread, *pphw);

    return 0;

error_exit:

    if (*pphw)
        free(*pphw);

    return -1;
}

//! destroys a hw
/*!
 * \param phw hw handle
 * \return 0 or negative error code
 */
int hw_close(hw_t *phw) {
    // stop receiver thread
    phw->rxthreadrunning = 0;
    pthread_join(phw->rxthread, NULL);
    
    datagram_pool_close(phw->tx_high);
    datagram_pool_close(phw->tx_low);

    if (phw)
        free(phw);

    return 0;
}

//! receiver thread
void *hw_rx_thread(void *arg) {
    hw_t *phw = (hw_t *)arg;
    uint8_t recv_frame[ETH_FRAME_LEN];
    ec_frame_t *pframe = (ec_frame_t *)recv_frame;
    struct sched_param param;
    int policy;

    // thread settings
    if (pthread_getschedparam(pthread_self(), &policy, &param) != 0)
        ec_log(10, "RX_THREAD", "error on pthread_getschedparam %s\n", strerror(errno));
    else {
        policy = SCHED_FIFO;
        param.sched_priority = phw->rxthreadprio;
        if (pthread_setschedparam(pthread_self(), policy, &param) != 0)
            ec_log(10, "RX_THREAD", "error on pthread_setschedparam %s\n", strerror(errno));
    }

#ifdef __VXWORKS__
    taskCpuAffinitySet(taskIdSelf(),  (cpuset_t)phw->rxthreadcpumask);
#elif defined __QNX__
    ThreadCtl(_NTO_TCTL_RUNMASK, (void *)phw->rxthreadcpumask);
#else
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (unsigned i = 0; i < (sizeof(phw->rxthreadcpumask)*8); ++i) 
        if (phw->rxthreadcpumask & (1 << i))
            CPU_SET(i, &cpuset);
        
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0)
        ec_log(10, "RX_THREAD", "error on pthread_setaffinity_np %s\n", strerror(errno));
#endif

    while (phw->rxthreadrunning) {
        ssize_t bytesrx = 
#ifdef __linux__
            recv(phw->sockfd, pframe, ETH_FRAME_LEN, 0);
#elif defined __VXWORKS__
            read(phw->sockfd, pframe, ETH_FRAME_LEN);
#else
#error unsupported OS
#endif
        if (bytesrx <= 0) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                continue;

            ec_log(10, "RX_THREAD", "recv: %s\n", strerror(errno));
            sleep(1);
        }
        
        /* check if it is an EtherCAT frame */
        if (pframe->ethertype != htons(ETH_P_ECAT)) {
            ec_log(10, "RX_THREAD", "received non-ethercat frame! (bytes %d, type 0x%X)\n", 
                    bytesrx, pframe->type);
            continue;
        }

        ec_datagram_t *d;
        for (d = ec_datagram_first(pframe); (uint8_t *)d < (uint8_t *)ec_frame_end(pframe);
                d = ec_datagram_next(d)) {
            datagram_entry_t *entry = phw->tx_send[d->idx];

            if (!entry) {
                ec_log(10, "RX_THREAD", "received idx %d, but we did not send one?\n", d->idx);
                continue;
            }

            memcpy(&entry->datagram, d, ec_datagram_length(d));
            if (entry->user_cb)
                (*entry->user_cb)(entry->user_arg, entry);
        }
    }

    return NULL;
}

static const uint8_t mac_dest[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
//static const uint8_t mac_src[]  = { 0x00, 0x1B, 0x21, 0xB8, 0x77, 0xCC };
static const uint8_t mac_src[]  = { 0x00, 0x30, 0x64, 0x0f, 0x83, 0x35 };

//! start sending queued ethercat datagrams
/*!
 * \param phw hardware handle
 * \return 0 or error code
 */
int hw_tx(hw_t *phw) {
    uint8_t send_frame[ETH_FRAME_LEN];
    memset(send_frame, 0, ETH_FRAME_LEN);
    ec_frame_t *pframe = (ec_frame_t *)send_frame;

    memcpy(pframe->mac_dest, mac_dest, 6);
    memcpy(pframe->mac_src , mac_src , 6);
    pframe->ethertype = htons(ETH_P_ECAT);
    pframe->type = 0x01;
    pframe->len = sizeof(ec_frame_t);

    ec_datagram_t *pdg = ec_datagram_first(pframe), *pdg_prev = NULL;
    size_t len;

    datagram_pool_t *pools[] = {
        phw->tx_high, phw->tx_low };
    int pool_idx = 0;

    // send high priority cyclic frames
    while (1) {
        if (pool_idx == 2)
            break;

        datagram_pool_get_next_len(pools[pool_idx], &len);

        if (((len == 0) && (pool_idx == 1)) || ((pframe->len + len) > phw->mtu_size)) { //ETH_FRAME_LEN)) {
            if (pframe->len == sizeof(ec_frame_t))
                break; // nothing to send

            // no more datagrams need to be sent or no more space in frame
            size_t bytestx = 
#ifdef __linux__
                send(phw->sockfd, pframe, pframe->len, 0);
#elif defined __VXWORKS__
                write(phw->sockfd, pframe, pframe->len);
#else
#error unsupported OS
#endif

            if (pframe->len != bytestx) {
                ec_log(10, "TX", "got only %d bytes out of %d bytes through.\n", 
                        bytestx, pframe->len);

                if (bytestx == -1)
                    ec_log(10, "TX", "error: %s\n", strerror(errno));
            }

            // reset length to send new frame
            pframe->len = sizeof(ec_frame_t);
            pdg = ec_datagram_first(pframe);
            continue;
        }

        if (len == 0) {
            pool_idx++;
            continue;
        }

        datagram_entry_t *entry;
        if (datagram_pool_get(pools[pool_idx], &entry, NULL) != 0)
            break;  // no more frames

        if (pdg_prev)
            ec_datagram_mark_next(pdg_prev);
        memcpy(pdg, &entry->datagram, ec_datagram_length(&entry->datagram));    
        pframe->len += ec_datagram_length(&entry->datagram);
        pdg_prev = pdg;
        pdg = ec_datagram_next(pdg);

        // store as sent
        phw->tx_send[entry->datagram.idx] = entry;
    }

    return 0;
}

