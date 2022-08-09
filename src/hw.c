/**
 * \file hw.c
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
 * If not, see <www.gnu.org/licenses/>.
 */

#include "config.h"

#ifdef HAVE_PTHREAD_SETAFFINITY_NP
// cppcheck-suppress misra-c2012-21.1
#define _GNU_SOURCE
#include <sched.h>
#endif

#include "libethercat/hw.h"
#include "libethercat/ec.h"
#include "libethercat/idx.h"
#include "libethercat/memory.h"
#include "libethercat/error_codes.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#ifdef HAVE_NET_IF_H
#include <net/if.h> 
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef __linux__

//#include <netpacket/packet.h>
#include <linux/if_packet.h>
#include <sys/mman.h>
#include <poll.h>

#elif defined __VXWORKS__
#include <vxWorks.h>
#include <taskLib.h>
#include <sys/ioctl.h>
#elif defined HAVE_NET_BPF_H
#include <sys/queue.h>
#include <net/bpf.h>
#include <net/if_types.h>
#ifdef __RTEMS__
#include <machine/rtems-bsd-kernel-space.h>
#include <machine/rtems-bsd-kernel-namespace.h>
#endif // __RTEMS__
#else
#error unsupported OS
#endif

// cppcheck-suppress misra-c2012-21.6
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define ETH_P_ECAT      0x88A4

#if defined HAVE_NET_BPF_H
#define ETH_FRAME_LEN 4096
#else
#define ETH_FRAME_LEN 1518
#endif

//! receiver thread forward declaration
static void *hw_rx_thread(void *arg);

#ifdef __linux__
#define RECEIVE(fd, frame, len)     recv((fd), (frame), (len), 0)
#define SEND(fd, frame, len)        send((fd), (frame), (len), 0)

// this need the grant_cap_net_raw kernel module 
// see https://gitlab.com/fastflo/open_ethercat
#define GRANT_CAP_NET_RAW_PROCFS "/proc/grant_cap_net_raw"

static int try_grant_cap_net_raw_init() {
    int ret = 0;

    if (access(GRANT_CAP_NET_RAW_PROCFS, R_OK) != 0) {
        // does probably not exist or is not readable
    } else {
        int fd = open(GRANT_CAP_NET_RAW_PROCFS, O_RDONLY);
        if (fd == -1) {
            ec_log(1, __func__, "error opening %s: %s\n", 
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

static int internal_hw_open(hw_t *phw, const osal_char_t *devname) {
    int ret = EC_OK;
    
    if (try_grant_cap_net_raw_init() == -1) {
        ec_log(10, "hw_open", "grant_cap_net_raw unsuccessfull, maybe we are "
                "not allowed to open a raw socket\n");
    }

    // create raw socket connection
    phw->sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ECAT));
    if (phw->sockfd <= 0) {
        ec_log(1, __func__, "socket error on opening SOCK_RAW: %s\n", strerror(errno));
        ret = EC_ERROR_UNAVAILABLE;
    }

    if ((ret == EC_OK) && (phw->mmap_packets > 0)) {
        int pagesize = getpagesize();
        ec_log(10, __func__, "got page size %d bytes\n", pagesize);

        struct tpacket_req tp;

        // tell kernel to export data through mmap()ped ring
        tp.tp_block_size = phw->mmap_packets * pagesize;
        tp.tp_block_nr   = 1;
        tp.tp_frame_size = pagesize;
        tp.tp_frame_nr   = phw->mmap_packets;
        if (setsockopt(phw->sockfd, SOL_PACKET, PACKET_RX_RING, (void*)&tp, sizeof(tp)) != 0) {
            ec_log(1, __func__, "setsockopt() rx ring: %s\n", strerror(errno));
            ret = EC_ERROR_UNAVAILABLE;
        } else if (setsockopt(phw->sockfd, SOL_PACKET, PACKET_TX_RING, (void*)&tp, sizeof(tp)) != 0) {
            ec_log(1, __func__, "setsockopt() tx ring: %s\n", strerror(errno));
            ret = EC_ERROR_UNAVAILABLE;
        } else {}

        if (ret == EC_OK) {
            // TODO unmap anywhere
            phw->rx_ring = mmap(0, phw->mmap_packets * pagesize * 2, PROT_READ | PROT_WRITE, MAP_SHARED, phw->sockfd, 0);
            phw->tx_ring = &phw->rx_ring[(phw->mmap_packets * pagesize)];

            phw->rx_ring_offset = 0;
            phw->tx_ring_offset = 0;
        }
    }

    if (ret == EC_OK) { 
        int i;

        // set timeouts
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 1;
        setsockopt(phw->sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(phw->sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        // do not route our frames
        i = 1;
        setsockopt(phw->sockfd, SOL_SOCKET, SO_DONTROUTE, &i, sizeof(i));

        // attach to out network interface
        struct ifreq ifr;
        (void)strcpy(ifr.ifr_name, devname);
        ioctl(phw->sockfd, SIOCGIFINDEX, &ifr);
        int ifindex = ifr.ifr_ifindex;
        (void)strcpy(ifr.ifr_name, devname);
        ifr.ifr_flags = 0;
        ioctl(phw->sockfd, SIOCGIFFLAGS, &ifr);
        ifr.ifr_flags = ifr.ifr_flags | IFF_PROMISC | IFF_BROADCAST | IFF_UP;
        /*int ret =*/ ioctl(phw->sockfd, SIOCSIFFLAGS, &ifr);
        //    if (ret != 0) {
        //        ec_log(1, __func__, "error setting interface %s: %s\n", devname, strerror(errno));
        //        goto error_exit;
        //    }

        ec_log(10, __func__, "binding raw socket to %s\n", devname);

        (void)memset(&ifr, 0, sizeof(ifr));
        (void)strncpy(ifr.ifr_name, devname, min(strlen(devname), IFNAMSIZ));
        ioctl(phw->sockfd, SIOCGIFMTU, &ifr);
        phw->mtu_size = ifr.ifr_mtu;
        ec_log(10, "hw_open", "got mtu size %d\n", phw->mtu_size);

        // bind socket to protocol, in this case RAW EtherCAT */
        struct sockaddr_ll sll;
        (void)memset(&sll, 0, sizeof(sll));
        sll.sll_family = AF_PACKET;
        sll.sll_ifindex = ifindex;
        sll.sll_protocol = htons(ETH_P_ECAT);
        bind(phw->sockfd, (struct sockaddr *) &sll, sizeof(sll));
    }

    return ret;
}
#elif defined __VXWORKS__ 
#define RECEIVE(fd, frame, len)     read((fd), (frame), (len))
#define SEND(fd, frame, len)        write((fd), (frame), (len))
    
static int internal_hw_open(hw_t *phw, const osal_char_t *devname) {
    int ret = EC_OK;

    /* we use snarf link layer device driver */
    // cppcheck-suppress misra-c2012-7.1
    phw->sockfd = open(devname, O_RDWR, 0644);
    if (phw->sockfd <= 0) {
        ec_log(1, __func__, "error opening %s: %s\n", devname, strerror(errno));
    } else {
        phw->mtu_size = 1480;
    }

    return ret;
}

#elif defined HAVE_NET_BPF_H
#define RECEIVE(fd, frame, len)     read((fd), (frame), (len))
#define SEND(fd, frame, len)        write((fd), (frame), (len))

// cppcheck-suppress misra-c2012-8.9
static struct bpf_insn insns[] = {                       
    BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 12),
    BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, ETH_P_ECAT, 0, 1),
    BPF_STMT(BPF_RET + BPF_K, (u_int)-1),
    BPF_STMT(BPF_RET + BPF_K, 0),
};

static int internal_hw_open(hw_t *phw, const osal_char_t *devname) {
    int ret = EC_OK;
    const unsigned int btrue = 1;
    const unsigned int bfalse = 0;

    int n = 0;
    struct ifreq bound_if;
    const osal_char_t bpf_devname[] = "/dev/bpf";

    (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);

    // open bpf device
    phw->sockfd = open(bpf_devname, O_RDWR, 0);
    if (phw->sockfd <= 0) {
        ec_log(1, __func__, "error opening bpf device %s: %s\n", bpf_devname, strerror(errno));
        ret = -1;
    } else {
        (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
        phw->mtu_size = 1480;

        // connect bpf to specified network device
        (void)snprintf(bound_if.ifr_name, IFNAMSIZ, devname);
        if (ioctl(phw->sockfd, BIOCSETIF, &bound_if) == -1 ) {
            ec_log(1, __func__, "error on BIOCSETIF: %s\n", 
                    strerror(errno));
            ret = -1;
        } else {
            (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
            // make sure we are dealing with an ethernet device.
            if (ioctl(phw->sockfd, BIOCGDLT, (caddr_t)&n) == -1) {
                ec_log(1, __func__, "error on BIOCGDLT: %s\n", 
                        strerror(errno));
                ret = -1;
            } else {
                (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
                // activate immediate mode (therefore, buf_len is initially set to "1")
                if (ioctl(phw->sockfd, BIOCIMMEDIATE, &btrue) == -1) {
                    ec_log(1, __func__, "error on BIOCIMMEDIATE: %s\n", 
                            strerror(errno));
                    ret = -1;
                } else {
                    (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
                    // request buffer length 
                    if (ioctl(phw->sockfd, BIOCGBLEN, &ETH_FRAME_LEN) == -1) {
                        ec_log(1, __func__, "error on BIOCGBLEN: %s\n", 
                                strerror(errno));
                        ret = -1;
                    } else {
                        (void)fprintf(stderr, "opening bpf device... %d, buf_isze is %d\n", __LINE__, ETH_FRAME_LEN);
                        static struct bpf_program my_bpf_program;
                        my_bpf_program.bf_len = sizeof(insns)/sizeof(insns[0]);
                        my_bpf_program.bf_insns = insns;

                        // setting filter to bpf
                        if (ioctl(phw->sockfd, BIOCSETF, &my_bpf_program) == -1) {
                            ec_log(1, __func__, "error on BIOCSETF: %s\n", 
                                    strerror(errno));
                            ret = -1;
                        } else {
                            (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
                            // we do not want to see the sent frames
                            if (ioctl(phw->sockfd, BIOCSSEESENT, &bfalse) == -1) {
                                ec_log(1, __func__, "error on BIOCSSEESENT: %s\n", 
                                        strerror(errno));
                                ret = -1;
                            } else {
                                (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
                                /* set receive call timeout */
                                static struct timeval timeout = { 0, 1000};
                                if (ioctl(phw->sockfd, BIOCSRTIMEOUT, &timeout) == -1) {
                                    ec_log(1, __func__, "error on BIOCSRTIMEOUT: %s\n", 
                                            strerror(errno));
                                    ret = -1;
                                } else {
                                    (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
                                    if (ioctl(phw->sockfd, BIOCFLUSH) == -1) {
                                        ec_log(1, __func__, "error on BIOCFLUSH: %s\n", 
                                                strerror(errno));
                                        ret = -1;
                                    } else {
                                        (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);

                                        osal_mutex_init(&phw->hw_lock, NULL);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return ret;
}
#else
#error unsupported OS
#endif

//! open a new hw
/*!
 * \param pphw return hw 
 * \param devname ethernet device name
 * \param prio receive thread prio
 * \param cpumask receive thread cpumask
 * \param mmap_packets  0 - using traditional send/recv, 1...n number of mmaped kernel packet buffers
 * \return 0 or negative error code
 */
int hw_open(hw_t **pphw, const osal_char_t *devname, int prio, int cpumask, int mmap_packets) {
    int ret = EC_OK;

    assert(pphw != NULL);

    // cppcheck-suppress misra-c2012-21.3
    (*pphw) = (hw_t *)ec_malloc(sizeof(hw_t));
    if ((*pphw) == NULL) {
        ret = EC_ERROR_OUT_OF_MEMORY;
    } else {

    (void)memset(*pphw, 0, sizeof(hw_t));
    
    (*pphw)->mmap_packets = mmap_packets;

    (void)pool_open(&(*pphw)->tx_high, 0, 1518);
    (void)pool_open(&(*pphw)->tx_low, 0, 1518);

    ret = internal_hw_open(*pphw, devname);
    }

    if (ret == EC_OK) {
        // thread settings
        (*pphw)->rxthreadprio = prio;
        (*pphw)->rxthreadcpumask = cpumask;
        (*pphw)->rxthreadrunning = 1;

        osal_task_attr_t attr;
        attr.priority = prio;
        attr.affinity = cpumask;
        osal_task_create(&(*pphw)->rxthread, &attr, hw_rx_thread, *pphw);
    }

    if ((ret != EC_OK) && (*pphw) != NULL) {
        // cppcheck-suppress misra-c2012-21.3
        ec_free(*pphw);
    }

    return ret;
}

//! destroys a hw
/*!
 * \param phw hw handle
 * \return 0 or negative error code
 */
int hw_close(hw_t *phw) {
    assert(phw != NULL);

    // stop receiver thread
    phw->rxthreadrunning = 0;
    osal_task_join(&phw->rxthread, NULL);

    osal_mutex_lock(&phw->hw_lock);
    (void)pool_close(phw->tx_high);
    (void)pool_close(phw->tx_low);

    osal_mutex_unlock(&phw->hw_lock);
    osal_mutex_destroy(&phw->hw_lock);

    // cppcheck-suppress misra-c2012-21.3
    ec_free(phw);

    return 0;
}

static void hw_process_rx_frame(hw_t *phw, ec_frame_t *pframe) {
    assert(phw != NULL);
    assert(pframe != NULL);

    /* check if it is an EtherCAT frame */
    if (pframe->ethertype != htons(ETH_P_ECAT)) {
        ec_log(1, "RX_THREAD", "received non-ethercat frame! (type 0x%X)\n", pframe->type);
    } else {
        ec_datagram_t *d;
        for (d = ec_datagram_first(pframe); 
                (osal_uint8_t *) d < (osal_uint8_t *) ec_frame_end(pframe);
                d = ec_datagram_next(d)) {
            pool_entry_t *entry = phw->tx_send[d->idx];

            if (!entry) {
                ec_log(1, "RX_THREAD", "received idx %d, but we did not send one?\n", d->idx);
                continue;
            }

            osal_size_t size = ec_datagram_length(d);
            if (entry->data_size < size) {
                ec_log(1, "RX_THREAD",
                        "received idx %d, size %d is to big for pool entry size %d!\n", 
                        d->idx, size, entry->data_size);
            }

            (void)memcpy(entry->data, (osal_uint8_t *)d, min(size, entry->data_size));

            if ((entry->user_cb) != NULL) {
                (*entry->user_cb)(entry->user_arg, entry);
            }
        }
    }
}

//! receiver thread
void *hw_rx_thread(void *arg) {
    // cppcheck-suppress misra-c2012-11.5
    hw_t *phw = (hw_t *) arg;
    osal_uint8_t recv_frame[ETH_FRAME_LEN];
    // cppcheck-suppress misra-c2012-11.3
    ec_frame_t *pframe = (ec_frame_t *) recv_frame;
//    struct sched_param param;
//    int policy;

    assert(phw != NULL);

//    // thread settings
//    if (pthread_getschedparam(pthread_self(), &policy, &param) != 0) {
//        ec_log(1, "RX_THREAD", "error on pthread_getschedparam %s\n",
//               strerror(errno));
//    } else {
//        policy = SCHED_FIFO;
//        param.sched_priority = phw->rxthreadprio;
//        if (pthread_setschedparam(pthread_self(), policy, &param) != 0) {
//            ec_log(1, "RX_THREAD", "error on pthread_setschedparam %s\n",
//                   strerror(errno));
//        }
//    }
//
//#ifdef __VXWORKS__
//    taskCpuAffinitySet(taskIdSelf(),  (cpuset_t)phw->rxthreadcpumask);
//#elif defined __QNX__
//    ThreadCtl(_NTO_TCTL_RUNMASK, (void *)phw->rxthreadcpumask);
//#else
//    cpu_set_t cpuset;
//    CPU_ZERO(&cpuset);
//    for (osal_uint32_t i = 0u; i < 32u; ++i) {
//        if ((phw->rxthreadcpumask & ((osal_uint32_t)1u << i)) != 0u) {
//            CPU_SET(i, &cpuset);
//        }
//    }
//
//#ifdef HAVE_PTHREAD_SETAFFINITY_NP
//    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
//        ec_log(1, "RX_THREAD", "error on pthread_setaffinity_np %s\n", 
//                strerror(errno));
//    }
//#endif
//
//#endif

    while (phw->rxthreadrunning != 0) {
#ifdef HAVE_NET_BPF_H
        osal_ssize_t bytesrx = read(phw->sockfd, pframe, ETH_FRAME_LEN);
        int local_errno = errno;
        
        if (bytesrx == -1) {
            if ((local_errno == EAGAIN) || (local_errno == EWOULDBLOCK) || (local_errno == EINTR)) {
                continue;
            }

            sleep(1);
            continue;
        }
        
        /* pointer to temporary bpf_frame if we have received more than one */
        osal_uint8_t *tmpframe2 = (osal_uint8_t *)pframe;

        while (bytesrx > 0) {
            /* calculate frame size, has to be aligned to word boundaries,
             * copy frame data to frame data buffer */
            osal_size_t bpf_frame_size = BPF_WORDALIGN(
                    // cppcheck-suppress misra-c2012-11.3
                    ((struct bpf_hdr *) tmpframe2)->bh_hdrlen +
                    // cppcheck-suppress misra-c2012-11.3
                    ((struct bpf_hdr *) tmpframe2)->bh_datalen);
            
            // cppcheck-suppress misra-c2012-11.3
            ec_frame_t *real_frame = (ec_frame_t *)(&tmpframe2[((struct bpf_hdr *)tmpframe2)->bh_hdrlen]);
            hw_process_rx_frame(phw, real_frame);

            /* values for next frame */
            bytesrx -= (osal_ssize_t)bpf_frame_size;
            tmpframe2 = &tmpframe2[bpf_frame_size];
        }
#else
        if (phw->mmap_packets > 0) {
            // using kernel mapped receive buffers
            // wait for received, non-processed packet
            struct pollfd pollset;
            pollset.fd = phw->sockfd;
            pollset.events = POLLIN;
            pollset.revents = 0;
            int ret = poll(&pollset, 1, -1);
            if (ret < 0) {
                continue;
            }
            
            int pagesize = getpagesize();

            struct tpacket_hdr *header;
            for (
                    // cppcheck-suppress misra-c2012-11.3
                    header = (struct tpacket_hdr *)(&phw->rx_ring[(phw->rx_ring_offset * pagesize)]);
                    header->tp_status & TP_STATUS_USER; 
                    // cppcheck-suppress misra-c2012-11.3
                    header = (struct tpacket_hdr *)(&phw->rx_ring[(phw->rx_ring_offset * pagesize)])) 
            {
                // cppcheck-suppress misra-c2012-11.3
                ec_frame_t *real_frame = (ec_frame_t *)(&((osal_char_t *)header)[header->tp_mac]);
                hw_process_rx_frame(phw, real_frame);

                header->tp_status = 0;
                phw->rx_ring_offset = (phw->rx_ring_offset + 1) % phw->mmap_packets;
            }
        } else {
            // using tradional recv function
            osal_ssize_t bytesrx = RECEIVE(phw->sockfd, pframe, ETH_FRAME_LEN);
            int local_errno = errno;

            if (bytesrx <= 0) {
                if ((local_errno == EAGAIN) || (local_errno == EWOULDBLOCK) || (local_errno == EINTR)) {
                    continue;
                }

                sleep(1);
                continue;
            }

            hw_process_rx_frame(phw, pframe);
        }
#endif
        
    }
    
    return NULL;
}

static struct tpacket_hdr *hw_get_next_tx_buffer(hw_t *phw) {
    struct tpacket_hdr *header;
    struct pollfd pollset;

    assert(phw != NULL);

    // cppcheck-suppress misra-c2012-11.3
    header = (struct tpacket_hdr *)(&phw->tx_ring[(phw->tx_ring_offset * getpagesize())]);

    while (header->tp_status != TP_STATUS_AVAILABLE) {
        // notify kernel
        if (send(phw->sockfd, NULL, 0, 0) < 0) {
            ec_log(1, __func__, "error on send: %s\n", strerror(errno));
        }

        // buffer not available, wait here...
        pollset.fd = phw->sockfd;
        pollset.events = POLLOUT;
        pollset.revents = 0;
        int ret = poll(&pollset, 1, 1000);
        if (ret < 0) {
            ec_log(1, __func__, "error on poll: %s\n", strerror(errno));
            continue;
        }
    }

    return header;
}

//! start sending queued ethercat datagrams
/*!
 * \param phw hardware handle
 * \return 0 or error code
 */
int hw_tx(hw_t *phw) {
    assert(phw != NULL);

    static const osal_uint8_t mac_dest[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    static const osal_uint8_t mac_src[] = {0x00, 0x30, 0x64, 0x0f, 0x83, 0x35};

    int ret = EC_OK;
    struct tpacket_hdr *header = NULL;
    osal_uint8_t send_frame[ETH_FRAME_LEN];
    // cppcheck-suppress misra-c2012-11.3
    ec_frame_t *pframe = (ec_frame_t *) send_frame;
    (void)memset(send_frame, 0, ETH_FRAME_LEN);

    osal_mutex_lock(&phw->hw_lock);

    if (phw->mmap_packets > 0) {
        header = hw_get_next_tx_buffer(phw);
        // cppcheck-suppress misra-c2012-11.3
        pframe = (ec_frame_t *)(&((osal_char_t *)header)[(TPACKET_HDRLEN - sizeof(struct sockaddr_ll))]);
    } else {
    }

    (void)memcpy(pframe->mac_dest, mac_dest, 6);
    (void)memcpy(pframe->mac_src, mac_src, 6);
    pframe->ethertype = htons(ETH_P_ECAT);
    pframe->type = 0x01;
    pframe->len = sizeof(ec_frame_t);

    ec_datagram_t *pdg = ec_datagram_first(pframe);
    ec_datagram_t *pdg_prev = NULL;

    pool_t *pools[] = {
            phw->tx_high, phw->tx_low};
    int pool_idx = 0;
    pool_entry_t *p_entry = NULL;
    ec_datagram_t *p_entry_dg = NULL;

    // send high priority cyclic frames
    while ((pool_idx < 2) && (ret == EC_OK)) {
        osal_size_t len = 0;
        (void)pool_peek(pools[pool_idx], &p_entry);
        if (p_entry !=  NULL) {
            // cppcheck-suppress misra-c2012-11.3
            p_entry_dg = (ec_datagram_t *)p_entry->data;
            len = ec_datagram_length(p_entry_dg);
        }

        if (((len == 0u) && (pool_idx == 1)) || ((pframe->len + len) > phw->mtu_size)) {
            if (pframe->len == sizeof(ec_frame_t)) {
                // nothing to send
            } else {
                if (phw->mmap_packets > 0) {
                    // fill header
                    header->tp_len = pframe->len;
                    header->tp_status = TP_STATUS_SEND_REQUEST;

                    // notify kernel
                    if (send(phw->sockfd, NULL, 0, 0) < 0) {
                        ec_log(1, __func__, "error on sendto: %s\n", strerror(errno));
                    }

                    // increase consumer ring pointer
                    phw->tx_ring_offset = (phw->tx_ring_offset + 1) % phw->mmap_packets;

                    // reset length to send new frame
                    header = hw_get_next_tx_buffer(phw);
                    // cppcheck-suppress misra-c2012-11.3
                    pframe = (ec_frame_t *)(&((osal_char_t *)header)[(TPACKET_HDRLEN - sizeof(struct sockaddr_ll))]);

                    // reset length to send new frame
                    (void)memcpy(pframe->mac_dest, mac_dest, 6);
                    (void)memcpy(pframe->mac_src, mac_src, 6);
                    pframe->ethertype = htons(ETH_P_ECAT);
                    pframe->type = 0x01;
                    pframe->len = sizeof(ec_frame_t);
                    pdg = ec_datagram_first(pframe);
                } else {
                    // no more datagrams need to be sent or no more space in frame
                    osal_ssize_t bytestx = SEND(phw->sockfd, pframe, pframe->len);

                    if ((osal_ssize_t)pframe->len != bytestx) {
                        ec_log(1, "TX", "got only %d bytes out of %d bytes "
                                "through.\n", bytestx, pframe->len);

                        if (bytestx == -1) {
                            ec_log(1, "TX", "error: %s\n", strerror(errno));
                        }
                    }

                    // reset length to send new frame
                    pframe->len = sizeof(ec_frame_t);
                    pdg = ec_datagram_first(pframe);
                }
            }
        }

        if (len == 0u) {
            pool_idx++;
        } else {
            ret = pool_get(pools[pool_idx], &p_entry, NULL);
            if (ret == EC_OK) {
                if (pdg_prev != NULL) {
                    ec_datagram_mark_next(pdg_prev);
                }

                // cppcheck-suppress misra-c2012-11.3
                p_entry_dg = (ec_datagram_t *)p_entry->data;
                (void)memcpy(pdg, p_entry_dg, ec_datagram_length(p_entry_dg));
                pframe->len += ec_datagram_length(p_entry_dg);
                pdg_prev = pdg;
                pdg = ec_datagram_next(pdg);

                // store as sent
                phw->tx_send[p_entry_dg->idx] = p_entry;
            }
        }
    }

    osal_mutex_unlock(&phw->hw_lock);

    return 0;
}

