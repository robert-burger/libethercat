/**
 * \file hw_bpf.c
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
#include <libethercat/hw.h>
#include <libethercat/ec.h>
#include <libethercat/idx.h>
#include <libethercat/error_codes.h>

#include <assert.h>
#include <sys/queue.h>
#include <linux/bpf.h>
#include <linux/filter.h>
//#include <net/bpf.h>
//#include <net/if_types.h>

#define RECEIVE(fd, frame, len)     read((fd), (frame), (len))
#define SEND(fd, frame, len)        write((fd), (frame), (len))

// cppcheck-suppress misra-c2012-8.9
static struct bpf_insn insns[] = {                       
    BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 12),
    BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, ETH_P_ECAT, 0, 1),
    BPF_STMT(BPF_RET + BPF_K, (u_int)-1),
    BPF_STMT(BPF_RET + BPF_K, 0),
};

// forward declarations
int hw_device_bpf_send(hw_t *phw, ec_frame_t *pframe);
int hw_device_bpf_recv(hw_t *phw);
void hw_device_bpf_send_finished(hw_t *phw);
int hw_device_bpf_get_tx_buffer(hw_t *phw, ec_frame_t **ppframe);
int hw_device_bpf_close(hw_common_t *phw);

//! Opens EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   devname     Null-terminated string to EtherCAT hw device name.
 *
 * \return 0 or negative error code
 */
int hw_device_bpf_open(hw_t *phw, const osal_char_t *devname) {
    int ret = EC_OK;
    const unsigned int btrue = 1;
    const unsigned int bfalse = 0;

    int n = 0;
    struct ifreq bound_if;
    const osal_char_t bpf_devname[] = (char[]){ "/dev/bpf" };

    (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
    
    phw->send = hw_device_bpf_send;
    phw->recv = hw_device_bpf_recv;
    phw->send_finished = hw_device_bpf_send_finished;
    phw->get_tx_buffer = hw_device_bpf_get_tx_buffer;
    phw->close = hw_device_bpf_close;

    // open bpf device
    phw->sockfd = open(bpf_devname, O_RDWR, 0);
    if (phw->sockfd <= 0) {
        ec_log(1, "HW_OPEN", "error opening bpf device %s: %s\n", bpf_devname, strerror(errno));
        ret = -1;
    } else {
        (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
        phw->mtu_size = 1480;

        // connect bpf to specified network device
        (void)snprintf(bound_if.ifr_name, IFNAMSIZ, devname);
        if (ioctl(phw->sockfd, BIOCSETIF, &bound_if) == -1 ) {
            ec_log(1, "HW_OPEN", "error on BIOCSETIF: %s\n", 
                    strerror(errno));
            ret = -1;
        } else {
            (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
            // make sure we are dealing with an ethernet device.
            if (ioctl(phw->sockfd, BIOCGDLT, (caddr_t)&n) == -1) {
                ec_log(1, "HW_OPEN", "error on BIOCGDLT: %s\n", 
                        strerror(errno));
                ret = -1;
            } else {
                (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
                // activate immediate mode (therefore, buf_len is initially set to "1")
                if (ioctl(phw->sockfd, BIOCIMMEDIATE, &btrue) == -1) {
                    ec_log(1, "HW_OPEN", "error on BIOCIMMEDIATE: %s\n", 
                            strerror(errno));
                    ret = -1;
                } else {
                    (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
                    // request buffer length 
                    if (ioctl(phw->sockfd, BIOCGBLEN, &ETH_FRAME_LEN) == -1) {
                        ec_log(1, "HW_OPEN", "error on BIOCGBLEN: %s\n", 
                                strerror(errno));
                        ret = -1;
                    } else {
                        (void)fprintf(stderr, "opening bpf device... %d, buf_isze is %d\n", __LINE__, ETH_FRAME_LEN);
                        static struct bpf_program my_bpf_program;
                        my_bpf_program.bf_len = sizeof(insns)/sizeof(insns[0]);
                        my_bpf_program.bf_insns = insns;

                        // setting filter to bpf
                        if (ioctl(phw->sockfd, BIOCSETF, &my_bpf_program) == -1) {
                            ec_log(1, "HW_OPEN", "error on BIOCSETF: %s\n", 
                                    strerror(errno));
                            ret = -1;
                        } else {
                            (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
                            // we do not want to see the sent frames
                            if (ioctl(phw->sockfd, BIOCSSEESENT, &bfalse) == -1) {
                                ec_log(1, "HW_OPEN", "error on BIOCSSEESENT: %s\n", 
                                        strerror(errno));
                                ret = -1;
                            } else {
                                (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
                                /* set receive call timeout */
                                static struct timeval timeout = { 0, 1000};
                                if (ioctl(phw->sockfd, BIOCSRTIMEOUT, &timeout) == -1) {
                                    ec_log(1, "HW_OPEN", "error on BIOCSRTIMEOUT: %s\n", 
                                            strerror(errno));
                                    ret = -1;
                                } else {
                                    (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
                                    if (ioctl(phw->sockfd, BIOCFLUSH) == -1) {
                                        ec_log(1, "HW_OPEN", "error on BIOCFLUSH: %s\n", 
                                                strerror(errno));
                                        ret = -1;
                                    } else {
                                        (void)fprintf(stderr, "opening bpf device... %d\n", __LINE__);
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

//! Close hardware layer
/*!
 * \param[in]   phw         Pointer to hw handle.
 *
 * \return 0 or negative error code
 */
int hw_device_bpf_close(struct hw_common *phw) {
    int ret = 0;

    struct hw_bpf *phw_bpf = container_of(phw, struct hw_bpf, common);
    close(phw_bpf->sockfd);

    return ret;
}


//! Receive a frame from an EtherCAT hw device.
/*!
 * \param[in]   phw         Pointer to hw handle. 
 * \param[in]   frame       Pointer to frame buffer.
 *
 * \return 0 or negative error code
 */
int hw_device_bpf_recv(hw_t *phw, ec_frame_t *pframe) {
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

    return EC_OK;
}

//! Doing internal stuff when finished sending frames
/*!
 * \param[in]   phw         Pointer to hw handle.
 */
void hw_device_bpf_send_finished(hw_t *phw) {
}


