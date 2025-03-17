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

#ifndef LIBETHERCAT_ERROR_CODES_H
#define LIBETHERCAT_ERROR_CODES_H

#include "libethercat/common.h"
#include "libethercat/mbx.h"

/** \defgroup error_codes_group Error Codes
 *
 * This modules contains libethercat error codes.
 *
 * @{
 */

#define EC_OK                                   (0)                                         //!< \brief All OK.
#define EC_ERROR_TIMER_EXPIRED                  (1)                                         //!< \brief Timer has expired.

#define EC_ERROR_GENERAL_MASK                   (0x00010000)                                //!< \brief General EtherCAT errors mask.
#define EC_ERROR_OUT_OF_MEMORY                  (EC_ERROR_GENERAL_MASK | 0x00000001)        //!< \brief System is out of memory.
#define EC_ERROR_WKC_MISMATCH                   (EC_ERROR_GENERAL_MASK | 0x00000002)        //!< \brief Working counter mismatch.
#define EC_ERROR_OUT_OF_INDICES                 (EC_ERROR_GENERAL_MASK | 0x00000010)        //!< \brief Out of EtherCAT datagram indices.
#define EC_ERROR_OUT_OF_DATAGRAMS               (EC_ERROR_GENERAL_MASK | 0x00000020)        //!< \brief Out of datagrams. See LEC_MAX_DATAGRAMS.
#define EC_ERROR_TIMEOUT                        (EC_ERROR_GENERAL_MASK | 0x00000040)        //!< \brief Timeout occured.
#define EC_ERROR_UNAVAILABLE                    (EC_ERROR_GENERAL_MASK | 0x00000080)        //!< \brief Resource currently unavailable.
#define EC_ERROR_HW_SEND                        (EC_ERROR_GENERAL_MASK | 0x00000100)        //!< \brief Hardware send error.
#define EC_ERROR_CYCLIC_LOOP                    (EC_ERROR_GENERAL_MASK | 0x00000200)        //!< \brief No cyclic loop running.


#define EC_ERROR_SLAVE_MASK                     (0x00020000)                                //!< \brief Slave error mask.
#define EC_ERROR_SLAVE_STATE_SWITCH             (EC_ERROR_SLAVE_MASK   | 0x00000001)        //!< \brief State switch on slave failed.
#define EC_ERROR_SLAVE_NOT_RESPONDING           (EC_ERROR_SLAVE_MASK   | 0x00000002)        //!< \brief Slave is not responding.
#define EC_ERROR_SLAVE_TRANSITION_ACTIVE        (EC_ERROR_SLAVE_MASK   | 0x00000004)        //!< \brief Slave state transition currently active.

#define EC_ERROR_EEPROM_MASK                    (0x00040000)                                //!< \brief Slave EEPROM error mask.
#define EC_ERROR_EEPROM_READ_ERROR              (EC_ERROR_EEPROM_MASK  | 0x00000001)        //!< \brief Slave EEPROM read error.
#define EC_ERROR_EEPROM_WRITE_ERROR             (EC_ERROR_EEPROM_MASK  | 0x00000002)        //!< \brief Slave EEPROM write error.
#define EC_ERROR_EEPROM_CHECKSUM                (EC_ERROR_EEPROM_MASK  | 0x00000008)        //!< \brief Slave EEPROM checksum wrong.
#define EC_ERROR_EEPROM_WRITE_IN_PROGRESS       (EC_ERROR_EEPROM_MASK  | 0x00000010)        //!< \brief Slave EEPROM write is in progress.
#define EC_ERROR_EEPROM_WRITE_ENABLE            (EC_ERROR_EEPROM_MASK  | 0x00000020)        //!< \brief Slave EEPROM write is not enabled.
#define EC_ERROR_EEPROM_CONTROL_TO_EC           (EC_ERROR_EEPROM_MASK  | 0x00000100)        //!< \brief Slave EEPROM error switching control to EtherCAT.
#define EC_ERROR_EEPROM_CONTROL_TO_PDI          (EC_ERROR_EEPROM_MASK  | 0x00000200)        //!< \brief Slave EEPROM error switching control to PDI.

#define EC_ERROR_HW_MASK                        (0x00080000)                                //!< \brief Hardware error mask.
#define EC_ERROR_HW_NOT_SUPPORTED               (EC_ERROR_HW_MASK      | 0x00000001)        //!< \brief Hardware not supported error.
#define EC_ERROR_HW_NO_INTERFACE                (EC_ERROR_HW_MASK      | 0x00000002)        //!< \brief No interface found.
#define EC_ERROR_HW_NO_LINK                     (EC_ERROR_HW_MASK      | 0x00000004)        //!< \brief No link detected on interface.                                                                                            

#define EC_ERROR_MAILBOX_MASK                   (0x00100000)                                //!< \brief General mailbox error mask.
#define EC_ERROR_MAILBOX_NOT_SUPPORTED_AOE      (EC_ERROR_MAILBOX_MASK | EC_MBX_AOE)        //!< \brief Mailbox AoE not supported.
#define EC_ERROR_MAILBOX_NOT_SUPPORTED_EOE      (EC_ERROR_MAILBOX_MASK | EC_MBX_EOE)        //!< \brief Mailbox EoE not supported.
#define EC_ERROR_MAILBOX_NOT_SUPPORTED_COE      (EC_ERROR_MAILBOX_MASK | EC_MBX_COE)        //!< \brief Mailbox CoE not supported.
#define EC_ERROR_MAILBOX_NOT_SUPPORTED_FOE      (EC_ERROR_MAILBOX_MASK | EC_MBX_FOE)        //!< \brief Mailbox FoE not supported.
#define EC_ERROR_MAILBOX_NOT_SUPPORTED_SOE      (EC_ERROR_MAILBOX_MASK | EC_MBX_SOE)        //!< \brief Mailbox SoE not supported.
#define EC_ERROR_MAILBOX_NOT_SUPPORTED_VOE      (EC_ERROR_MAILBOX_MASK | EC_MBX_VOE)        //!< \brief Mailbox VoE not supported.

#define EC_ERROR_MAILBOX_READ_IS_NULL           (EC_ERROR_MAILBOX_MASK | 0x00000010)        //!< \brief Mailbox read mailbox is NULL.
#define EC_ERROR_MAILBOX_WRITE_IS_NULL          (EC_ERROR_MAILBOX_MASK | 0x00000020)        //!< \brief Mailbox write mailbox is NULL.

#define EC_ERROR_MAILBOX_READ_EMPTY             (EC_ERROR_MAILBOX_MASK | 0x00000100)        //!< \brief Mailbox reading mailbox failed (was empty).
#define EC_ERROR_MAILBOX_WRITE_FULL             (EC_ERROR_MAILBOX_MASK | 0x00000200)        //!< \brief Mailbox writing mailbox failed.

#define EC_ERROR_MAILBOX_TIMEOUT                (EC_ERROR_MAILBOX_MASK | 0x00001000)        //!< \brief Mailbox timeout occured.
#define EC_ERROR_MAILBOX_ABORT                  (EC_ERROR_MAILBOX_MASK | 0x00002000)        //!< \brief Mailbox got mailbox abort from slave.
#define EC_ERROR_MAILBOX_READ                   (EC_ERROR_MAILBOX_MASK | 0x00004000)        //!< \brief Mailbox error on reading mailbox.
#define EC_ERROR_MAILBOX_WRITE                  (EC_ERROR_MAILBOX_MASK | 0x00008000)        //!< \brief Mailbox error on writing mailbox.

#define EC_ERROR_MAILBOX_OUT_OF_SEND_BUFFERS    (EC_ERROR_MAILBOX_MASK | 0x00010000)        //!< \brief Mailbox ran out of read buffers. See LEC_MAX_MBX_ENTRIES.
#define EC_ERROR_MAILBOX_OUT_OF_WRITE_BUFFERS   (EC_ERROR_MAILBOX_MASK | 0x00020000)        //!< \brief Mailbox ran out of write buffers. See LEC_MAX_MBX_ENTRIES.
#define EC_ERROR_MAILBOX_BUFFER_TOO_SMALL       (EC_ERROR_MAILBOX_MASK | 0x00040000)        //!< \brief Mailbox buffer is too small.

#define EC_ERROR_MAILBOX_COE_MASK               (0x00200000)                                //!< \brief CoE mailbox error mask.
#define EC_ERROR_MAILBOX_COE_INDEX_NOT_FOUND    (EC_ERROR_MAILBOX_COE_MASK | 0x00000001)    //!< \brief Mailbox CoE
#define EC_ERROR_MAILBOX_COE_SUBINDEX_NOT_FOUND (EC_ERROR_MAILBOX_COE_MASK | 0x00000002)    //!< \brief Mailbox CoE

#define EC_ERROR_MAILBOX_FOE_MASK               (0x00800000)                                //!< \brief FoE mailbox error mask.
#define EC_ERROR_MAILBOX_FOE_ERROR_REQ          (EC_ERROR_MAILBOX_FOE_MASK | 0x00000001)    //!< \brief Mailbox FoE
#define EC_ERROR_MAILBOX_FOE_NO_ACK             (EC_ERROR_MAILBOX_FOE_MASK | 0x00000002)    //!< \brief Mailbox FoE
#define EC_ERROR_MAILBOX_FOE_AGAIN              (EC_ERROR_MAILBOX_FOE_MASK | 0x00000004)    //!< \brief Mailbox FoE

/** @} */

#endif // LIBETHERCAT_ERROR_CODES_H

