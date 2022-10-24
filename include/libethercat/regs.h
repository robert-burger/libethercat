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

#ifndef LIBETHERCAT_REGS_H
#define LIBETHERCAT_REGS_H

/** Ethercat registers */

enum {
    EC_CMD_NOP          = 0x00,  //!< no op
    EC_CMD_APRD         = 0x01,  //!< auto increment read
    EC_CMD_APWR         = 0x02,  //!< auto increment write
    EC_CMD_APRW         = 0x03,  //!< auto increment read write
    EC_CMD_FPRD         = 0x04,  //!< configured address read
    EC_CMD_FPWR         = 0x05,  //!< configured address write
    EC_CMD_FPRW         = 0x06,  //!< configured address read write
    EC_CMD_BRD          = 0x07,  //!< broadcast read
    EC_CMD_BWR          = 0x08,  //!< broaddcast write
    EC_CMD_BRW          = 0x09,  //!< broadcast read write
    EC_CMD_LRD          = 0x0A,  //!< logical memory read
    EC_CMD_LWR          = 0x0B,  //!< logical memory write
    EC_CMD_LRW          = 0x0C,  //!< logical memory read write
    EC_CMD_ARMW         = 0x0D,  //!< auto increment read mulitple write
    EC_CMD_FRMW         = 0x0E   //!< configured read mulitple write
};

#define EC_REG_TYPE             (0x0000u)
#define EC_REG_SM_FFMU_CH       (0x0004u)
#define EC_REG_FMMU_CH          (0x0004u)
#define EC_REG_SM_CH            (0x0005u)
#define EC_REG_RAM_SIZE         (0x0006u)
#define EC_REG_PORTDES          (0x0007u)
#define EC_REG_ESCSUP           (0x0008u)
#define EC_REG_STADR            (0x0010u)
#define EC_REG_ALIAS            (0x0012u)
#define EC_REG_DLCTL            (0x0100u)
#define EC_REG_DLPORT           (0x0101u)
#define EC_REG_DLALIAS          (0x0103u)
#define EC_REG_DLSTAT           (0x0110u)
#define EC_REG_ALCTL            (0x0120u)
#define EC_REG_ALSTAT           (0x0130u)
#define EC_REG_ALSTATCODE       (0x0134u)
#define EC_REG_PDICTL           (0x0140u)
#define EC_REG_IRQMASK          (0x0200u)
#define EC_REG_RXERR            (0x0300u)
#define EC_REG_EEPCFG           (0x0500u)
#define EC_REG_EEPCTL           (0x0502u)
#define EC_REG_EEPSTAT          (0x0502u)
#define EC_REG_EEPADR           (0x0504u)
#define EC_REG_EEPDAT           (0x0508u)
#define EC_REG_MII_CTRLSTAT     (0x0510u)
#define EC_REG_MII_PHY_ADR      (0x0512u)
#define EC_REG_MII_PHY_REG      (0x0513u)
#define EC_REG_MII_PHY_DATA     (0x0514u)
#define EC_REG_MII_ECAT_ACC     (0x0516u)
#define EC_REG_MII_PDI_ACC      (0x0517u)
#define EC_REG_MII_PHY0_ST      (0x0518u)
#define EC_REG_MII_PHY1_ST      (0x0519u)
#define EC_REG_MII_PHY2_ST      (0x051Au)
#define EC_REG_MII_PHY3_ST      (0x051Bu)
#define EC_REG_FMMU0            (0x0600u)
#define EC_REG_FMMU1            (EC_REG_FMMU0 + 0x10u)
#define EC_REG_FMMU2            (EC_REG_FMMU1 + 0x10u)
#define EC_REG_FMMU3            (EC_REG_FMMU2 + 0x10u)
#define EC_REG_SM0              (0x0800u)
#define EC_REG_SM1              (EC_REG_SM0 + 0x08u)
#define EC_REG_SM2              (EC_REG_SM1 + 0x08u)
#define EC_REG_SM3              (EC_REG_SM2 + 0x08u)
#define EC_REG_SM0STAT          (EC_REG_SM0 + 0x05u)
#define EC_REG_SM0ACT           (EC_REG_SM0 + 0x06u)
#define EC_REG_SM0CONTR         (EC_REG_SM0 + 0x07u)
#define EC_REG_SM1STAT          (EC_REG_SM1 + 0x05u)
#define EC_REG_SM1ACT           (EC_REG_SM1 + 0x06u)
#define EC_REG_SM1CONTR         (EC_REG_SM1 + 0x07u)
#define EC_REG_DCTIME0          (0x0900u)
#define EC_REG_DCTIME1          (0x0904u)
#define EC_REG_DCTIME2          (0x0908u)
#define EC_REG_DCTIME3          (0x090Cu)
#define EC_REG_DCSYSTIME        (0x0910u)
#define EC_REG_DCSOF            (0x0918u)
#define EC_REG_DCSYSOFFSET      (0x0920u)
#define EC_REG_DCSYSDELAY       (0x0928u)
#define EC_REG_DCSYSDIFF        (0x092Cu)
#define EC_REG_DCSPEEDCNT       (0x0930u)
#define EC_REG_DCTIMEFILT       (0x0934u)
#define EC_REG_DCCUC            (0x0980u)
#define EC_REG_DCSYNCACT        (0x0981u)
#define EC_REG_DCSTART0         (0x0990u)
#define EC_REG_DCCYCLE0         (0x09A0u)
#define EC_REG_DCCYCLE1         (0x09A4u)

#endif // LIBETHERCAT_REGS_H

