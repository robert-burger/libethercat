/**
 * \file error_codes.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 13 Mar 2017
 *
 * \brief ethercat master error codes
 *
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

#ifndef LIBETHERCAT_ERROR_CODES_H
#define LIBETHERCAT_ERROR_CODES_H

#include "libethercat/common.h"
#include "libethercat/mbx.h"

#define EC_OK                                   0

#define EC_ERROR_GENERAL_MASK                   0x00010000

#define EC_ERROR_OUT_OF_MEMORY                  (EC_ERROR_GENERAL_MASK | 0x00000001)
#define EC_ERROR_WKC_MISMATCH                   (EC_ERROR_GENERAL_MASK | 0x00000002)
#define EC_ERROR_OUT_OF_INDICES                 (EC_ERROR_GENERAL_MASK | 0x00000010)
#define EC_ERROR_OUT_OF_DATAGRAMS               (EC_ERROR_GENERAL_MASK | 0x00000020)
#define EC_ERROR_TIMEOUT                        (EC_ERROR_GENERAL_MASK | 0x00000040)
#define EC_ERROR_UNAVAILABLE                    (EC_ERROR_GENERAL_MASK | 0x00000080)

#define EC_ERROR_SLAVE_MASK                     0x00020000
#define EC_ERROR_SLAVE_STATE_SWITCH             (EC_ERROR_SLAVE_MASK   | 0x00000001)
#define EC_ERROR_SLAVE_NOT_RESPONDING           (EC_ERROR_SLAVE_MASK   | 0x00000002)

#define EC_ERROR_MAILBOX_MASK                   0x00100000

#define EC_ERROR_MAILBOX_NOT_SUPPORTED_AOE      (EC_ERROR_MAILBOX_MASK | EC_MBX_AOE)
#define EC_ERROR_MAILBOX_NOT_SUPPORTED_EOE      (EC_ERROR_MAILBOX_MASK | EC_MBX_EOE)
#define EC_ERROR_MAILBOX_NOT_SUPPORTED_COE      (EC_ERROR_MAILBOX_MASK | EC_MBX_FOE)
#define EC_ERROR_MAILBOX_NOT_SUPPORTED_FOE      (EC_ERROR_MAILBOX_MASK | EC_MBX_COE)
#define EC_ERROR_MAILBOX_NOT_SUPPORTED_SOE      (EC_ERROR_MAILBOX_MASK | EC_MBX_SOE)
#define EC_ERROR_MAILBOX_NOT_SUPPORTED_VOE      (EC_ERROR_MAILBOX_MASK | EC_MBX_VOE)

#define EC_ERROR_MAILBOX_READ_IS_NULL           (EC_ERROR_MAILBOX_MASK | 0x00000010)
#define EC_ERROR_MAILBOX_WRITE_IS_NULL          (EC_ERROR_MAILBOX_MASK | 0x00000020)

#define EC_ERROR_MAILBOX_READ_EMPTY             (EC_ERROR_MAILBOX_MASK | 0x00000100)
#define EC_ERROR_MAILBOX_WRITE_FULL             (EC_ERROR_MAILBOX_MASK | 0x00000200)

#define EC_ERROR_MAILBOX_TIMEOUT                (EC_ERROR_MAILBOX_MASK | 0x00001000)
#define EC_ERROR_MAILBOX_ABORT                  (EC_ERROR_MAILBOX_MASK | 0x00002000)
#define EC_ERROR_MAILBOX_READ                   (EC_ERROR_MAILBOX_MASK | 0x00004000)
#define EC_ERROR_MAILBOX_WRITE                  (EC_ERROR_MAILBOX_MASK | 0x00008000)

#define EC_ERROR_MAILBOX_OUT_OF_SEND_BUFFERS    (EC_ERROR_MAILBOX_MASK | 0x00000001)
#define EC_ERROR_MAILBOX_OUT_OF_WRITE_BUFFERS   (EC_ERROR_MAILBOX_MASK | 0x00000002)
#define EC_ERROR_MAILBOX_BUFFER_TOO_SMALL       (EC_ERROR_MAILBOX_MASK | 0x00000004)

#define EC_ERROR_MAILBOX_FOE_MASK               0x00800000

#define EC_ERROR_MAILBOX_FOE_ERROR_REQ          (EC_ERROR_MAILBOX_FOE_MASK | 0x00000001)
#define EC_ERROR_MAILBOX_FOE_NO_ACK             (EC_ERROR_MAILBOX_FOE_MASK | 0x00000002)
#define EC_ERROR_MAILBOX_FOE_AGAIN              (EC_ERROR_MAILBOX_FOE_MASK | 0x00000004)

#endif // LIBETHERCAT_ERROR_CODES_H

