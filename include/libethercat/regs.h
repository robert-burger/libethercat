/**
 * \file pool.h
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Sep 2022
 *
 * \brief EtherCAT master slave registers
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

#ifndef LIBETHERCAT_REGS_H
#define LIBETHERCAT_REGS_H

#include <libosal/types.h>

/** \defgroup regs_group Registers
 *
 * This modules contains EtherCAT register defines.
 *
 * @{
 */

/** Ethercat registers */

enum {
    EC_CMD_NOP = 0x00,   //!< \brief no op
    EC_CMD_APRD = 0x01,  //!< \brief auto increment read
    EC_CMD_APWR = 0x02,  //!< \brief auto increment write
    EC_CMD_APRW = 0x03,  //!< \brief auto increment read write
    EC_CMD_FPRD = 0x04,  //!< \brief configured address read
    EC_CMD_FPWR = 0x05,  //!< \brief configured address write
    EC_CMD_FPRW = 0x06,  //!< \brief configured address read write
    EC_CMD_BRD = 0x07,   //!< \brief broadcast read
    EC_CMD_BWR = 0x08,   //!< \brief broaddcast write
    EC_CMD_BRW = 0x09,   //!< \brief broadcast read write
    EC_CMD_LRD = 0x0A,   //!< \brief logical memory read
    EC_CMD_LWR = 0x0B,   //!< \brief logical memory write
    EC_CMD_LRW = 0x0C,   //!< \brief logical memory read write
    EC_CMD_ARMW = 0x0D,  //!< \brief auto increment read mulitple write
    EC_CMD_FRMW = 0x0E   //!< \brief configured read mulitple write
};

#define EC_REG_TYPE (0x0000u)        //!< \brief Type
#define EC_REG_SM_FFMU_CH (0x0004u)  //!< \brief Sync manager and FMMU channels
#define EC_REG_FMMU_CH (0x0004u)     //!< \brief FMMU channels
#define EC_REG_SM_CH (0x0005u)       //!< \brief Sync manager channels
#define EC_REG_RAM_SIZE (0x0006u)    //!< \brief RAM size
#define EC_REG_PORTDES (0x0007u)     //!< \brief Port description
#define EC_REG_ESCSUP (0x0008u)      //!< \brief ESC support

#define EC_REG_ESCSUP__FMMU_BIT_OP_NOT_SUPP (0x0001u)  //!< \brief FMMU bit not supported in OP
#define EC_REG_ESCSUP__NO_SUPP_RESERVED_REG (0x0002u)  //!< \brief Reserver regs not supported
#define EC_REG_ESCSUP__DC_SUPP (0x0004u)               //!< \brief Distributed clock support
#define EC_REG_ESCSUP__DC_RANGE (0x0008u)              //!< \brief Distributed clock 64-bit support
#define EC_REG_ESCSUP__LOW_J_EBUS (0x0010u)            //!< \brief Low jitter on EBUS
#define EC_REG_ESCSUP__ENH_LD_EBUS (0x0020u)           //!< \brief Enhanced link detection on EBUS
#define EC_REG_ESCSUP__ENH_LD_MII (0x0040u)            //!< \brief Enhanced link detection on MII
#define EC_REG_ESCSUP__FCS_S_ERR (0x0080u)             //!< \brief Frame checksum error counter
#define EC_REG_ESCSUP__ENHANCED_DC_SYNC_ACT \
    (0x0100u)  //!< \brief Enhanced distributed clock sync activation
#define EC_REG_ESCSUP__NOT_SUPP_LRW (0x0200u)    //!< \brief LRW not supported
#define EC_REG_ESCSUP__NOT_SUPP_BAFRW (0x0400u)  //!< \brief BAFRW not supporte
#define EC_REG_ESCSUP__S_FMMU_SYMC (0x0800u)     //!< \brief FMMU sync

#define EC_REG_STADR (0x0010u)                //!< \brief Station address.
#define EC_REG_ALIAS (0x0012u)                //!< \brief Station alias.
#define EC_REG_DLCTL (0x0100u)                //!< \brief Data-link-layer control
#define EC_REG_DLPORT (0x0101u)               //!< \brief Data-link-layer port
#define EC_REG_DLALIAS (0x0103u)              //!< \brief Data-link-layer alias
#define EC_REG_DLSTAT (0x0110u)               //!< \brief Data-link-layer status
#define EC_REG_ALCTL (0x0120u)                //!< \brief Application-layer control
#define EC_REG_ALSTAT (0x0130u)               //!< \brief Application-layer status
#define EC_REG_ALSTATCODE (0x0134u)           //!< \brief Application-layer status code
#define EC_REG_PDICTL (0x0140u)               //!< \brief PDI control
#define EC_REG_IRQMASK (0x0200u)              //!< \brief IRQ mask
#define EC_REG_RXERR (0x0300u)                //!< \brief RX error
#define EC_REG_EEPCFG (0x0500u)               //!< \brief EEPROM config
#define EC_REG_EEPCTL (0x0502u)               //!< \brief EEPROM control
#define EC_REG_EEPSTAT (0x0502u)              //!< \brief EEPROM status
#define EC_REG_EEPADR (0x0504u)               //!< \brief EEPROM address
#define EC_REG_EEPDAT (0x0508u)               //!< \brief EEPROM data
#define EC_REG_MII_CTRLSTAT (0x0510u)         //!< \brief MII control/status
#define EC_REG_MII_PHY_ADR (0x0512u)          //!< \brief MII phy address
#define EC_REG_MII_PHY_REG (0x0513u)          //!< \brief MII phy register
#define EC_REG_MII_PHY_DATA (0x0514u)         //!< \brief MII phy data
#define EC_REG_MII_ECAT_ACC (0x0516u)         //!< \brief MII ECAT ACC
#define EC_REG_MII_PDI_ACC (0x0517u)          //!< \brief MII PDI ACC
#define EC_REG_MII_PHY0_ST (0x0518u)          //!< \brief MII phy0 status
#define EC_REG_MII_PHY1_ST (0x0519u)          //!< \brief MII phy1 status
#define EC_REG_MII_PHY2_ST (0x051Au)          //!< \brief MII phy2 status
#define EC_REG_MII_PHY3_ST (0x051Bu)          //!< \brief MII phy3 status
#define EC_REG_FMMU0 (0x0600u)                //!< \brief FMMU0 base
#define EC_REG_FMMU1 (EC_REG_FMMU0 + 0x10u)   //!< \brief FMMU1 base
#define EC_REG_FMMU2 (EC_REG_FMMU1 + 0x10u)   //!< \brief FMMU2 base
#define EC_REG_FMMU3 (EC_REG_FMMU2 + 0x10u)   //!< \brief FMMU3 base
#define EC_REG_SM0 (0x0800u)                  //!< \brief Sync manager 0 base
#define EC_REG_SM1 (EC_REG_SM0 + 0x08u)       //!< \brief Sync manager 1 base
#define EC_REG_SM2 (EC_REG_SM1 + 0x08u)       //!< \brief Sync manager 2 base
#define EC_REG_SM3 (EC_REG_SM2 + 0x08u)       //!< \brief Sync manager 3 base
#define EC_REG_SM0STAT (EC_REG_SM0 + 0x05u)   //!< \brief Sync manager 0 status
#define EC_REG_SM0ACT (EC_REG_SM0 + 0x06u)    //!< \brief Sync manager 0 active
#define EC_REG_SM0CONTR (EC_REG_SM0 + 0x07u)  //!< \brief Sync manager 0 control
#define EC_REG_SM1STAT (EC_REG_SM1 + 0x05u)   //!< \brief Sync manager 1 status
#define EC_REG_SM1ACT (EC_REG_SM1 + 0x06u)    //!< \brief Sync manager 1 active
#define EC_REG_SM1CONTR (EC_REG_SM1 + 0x07u)  //!< \brief Sync manager 1 control
#define EC_REG_DCTIME0 (0x0900u)              //!< \brief Distributed clock port time 0
#define EC_REG_DCTIME1 (0x0904u)              //!< \brief Distributed clock port time 1
#define EC_REG_DCTIME2 (0x0908u)              //!< \brief Distributed clock port time 2
#define EC_REG_DCTIME3 (0x090Cu)              //!< \brief Distributed clock port time 3
#define EC_REG_DCSYSTIME (0x0910u)            //!< \brief Distributed clock system time
#define EC_REG_DCSOF (0x0918u)                //!< \brief Distributed clock offset
#define EC_REG_DCSYSOFFSET (0x0920u)          //!< \brief Distributed clock system time offset
#define EC_REG_DCSYSDELAY (0x0928u)           //!< \brief Distributed clock system time delay
#define EC_REG_DCSYSDIFF (0x092Cu)            //!< \brief Distributed clock system time difference
#define EC_REG_DCSPEEDCNT (0x0930u)           //!< \brief Distributed clock speed count
#define EC_REG_DCTIMEFILT (0x0934u)           //!< \brief Distributed clock time filter
#define EC_REG_DCCUC (0x0980u)                //!< \brief Distributed clock CUC
#define EC_REG_DCSYNCACT (0x0981u)            //!< \brief Distributed clock sync activation
#define EC_REG_DCSTART0 (0x0990u)             //!< \brief Distributed clock start time
#define EC_REG_DCCYCLE0 (0x09A0u)             //!< \brief Distributed clock Sync0 cycletime
#define EC_REG_DCCYCLE1 (0x09A4u)             //!< \brief Distributed clock Sync1 cycletime

typedef struct ec_reg_dl_status {
    osal_uint16_t pdi_operational : 1;          //!< \brief PDI operational.
    osal_uint16_t dls_user_watchdog_tatus : 1;  //!< \brief DLS user watchdog status.
    osal_uint16_t extended_link_detection : 1;  //!< \brief Extended link detection flag.
    osal_uint16_t reserved_1 : 1;               //!< \brief for future use.
    osal_uint16_t link_status_port_0 : 1;       //!< \brief Link status of port 0.
    osal_uint16_t link_status_port_1 : 1;       //!< \brief Link status of port 1.
    osal_uint16_t link_status_port_2 : 1;       //!< \brief Link status of port 2.
    osal_uint16_t link_status_port_3 : 1;       //!< \brief Link status of port 3.
    osal_uint16_t loop_status_port_0 : 1;       //!< \brief Loop status of port 0.
    osal_uint16_t signal_detection_port_0 : 1;  //!< \brief Signal detected on port 0.
    osal_uint16_t loop_status_port_1 : 1;       //!< \brief Loop status of port 1.
    osal_uint16_t signal_detection_port_1 : 1;  //!< \brief Signal detected on port 1.
    osal_uint16_t loop_status_port_2 : 1;       //!< \brief Loop status of port 2.
    osal_uint16_t signal_detection_port_2 : 1;  //!< \brief Signal detected on port 2.
    osal_uint16_t loop_status_port_3 : 1;       //!< \brief Loop status of port 3.
    osal_uint16_t signal_detection_port_3 : 1;  //!< \brief Signal detected on port 3.
} ec_reg_dl_status_t;                           //!< \brief DL status register type.

/** @} */

#endif  // LIBETHERCAT_REGS_H
