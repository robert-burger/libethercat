/**
 * \file mii.c
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief ethercat mii access fuctions
 *
 * These functions are used to ensure access to the EtherCAT
 * slaves MII.
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

#include "libethercat/mii.h"
#include "libethercat/ec.h"

#include <string.h>
#include <stdio.h>

#define fp_check(op, reg, buf, buf_len) {                                           \
    ec_fp ## op(pec, pec->slaves[slave].fixed_address, reg, buf, buf_len, &wkc);    \
    if (wkc != 1) {                                                                 \
        ec_log(10, __func__, "slave %2d did not respond %s register 0x%X\n",        \
                slave, strcmp(#op, "rd") == 0 ? "reading" : "writing", reg);        \
        return -1; }}

// Read 16-bit word via MII.
/*
 * \param[in] pec               Pointer to EtherCAT master structure, 
 *                              which you got from \link ec_open \endlink.
 * \param[in] slave             Number of EtherCAT slave. this depends on 
 *                              the physical order of the EtherCAT slaves 
 *                              (usually the n'th slave attached).
 * \param[in] phy_adr           Address of PHY attached via MII.
 * \param[in] phy_reg           Register of PHY selected by \p phy_adr.
 * \param[out] data             Returns read 16-bit data value.
 *
 * \retval 0    On success
 */
int ec_miiread(struct ec *pec, uint16_t slave, 
        uint8_t phy_adr, uint16_t phy_reg, uint16_t *data) {
    uint16_t wkc;
    uint16_t phy_adr_reg = phy_adr | ((uint16_t)phy_reg << 8);
    uint16_t ctrl_stat = 0;

    // write phy address amd regoster
    fp_check(wr, EC_REG_MII_PHY_ADR, (uint8_t *)&phy_adr_reg, sizeof(phy_adr_reg));

    // execute read command
    fp_check(rd, EC_REG_MII_CTRLSTAT, (uint8_t *)&ctrl_stat, sizeof(ctrl_stat));
    ctrl_stat = (ctrl_stat & ~0x0300) | 0x0100;
    fp_check(wr, EC_REG_MII_CTRLSTAT, (uint8_t *)&ctrl_stat, sizeof(ctrl_stat));

    // set up timeout 100 ms
    ec_timer_t timeout;
    ec_timer_init(&timeout, 10000000000);

    do {
        fp_check(rd, EC_REG_MII_CTRLSTAT, (uint8_t *)&ctrl_stat, sizeof(ctrl_stat));

        if (ec_timer_expired(&timeout)) {
            ec_log(10, __func__, "slave %2d did not respond on MII command\n", slave);
            return -1;
        }
    } while (ctrl_stat & 0x8000);

        
    fp_check(rd, EC_REG_MII_PHY_DATA, (uint8_t *)data, sizeof(*data));
    return 0;
}

// Write 16-bit word via MII.
/*
 * \param[in] pec               Pointer to EtherCAT master structure, 
 *                              which you got from \link ec_open \endlink.
 * \param[in] slave             Number of EtherCAT slave. this depends on 
 *                              the physical order of the EtherCAT slaves 
 *                              (usually the n'th slave attached).
 * \param[in] phy_adr           Address of PHY attached via MII.
 * \param[in] phy_reg           Register of PHY selected by \p phy_adr.
 * \param[in] data              Data contains 16-bit data value to write.
 *
 * \retval 0    On success
 */
int ec_miiwrite(struct ec *pec, uint16_t slave, 
        uint8_t phy_adr, uint16_t phy_reg, uint16_t *data) {
    uint16_t wkc;
    uint16_t phy_adr_reg = phy_adr | ((uint16_t)phy_reg << 8);
    uint16_t ctrl_stat = 0;

    // write phy address, register and data
    fp_check(wr, EC_REG_MII_PHY_ADR, (uint8_t *)&phy_adr_reg, sizeof(phy_adr_reg));      
    fp_check(wr, EC_REG_MII_PHY_DATA, (uint8_t *)data, sizeof(*data));

    // execute write command
    fp_check(rd, EC_REG_MII_CTRLSTAT, (uint8_t *)&ctrl_stat, sizeof(ctrl_stat));
    ctrl_stat = (ctrl_stat & ~0x0300) | 0x0201;
    fp_check(wr, EC_REG_MII_CTRLSTAT, (uint8_t *)&ctrl_stat, sizeof(ctrl_stat));

    // set up timeout 100 ms
    ec_timer_t timeout;
    ec_timer_init(&timeout, 100000000);

    do {
        fp_check(rd, EC_REG_MII_CTRLSTAT, (uint8_t *)&ctrl_stat, sizeof(ctrl_stat));

        if (ec_timer_expired(&timeout)) {
            ec_log(10, __func__, "slave %2d did not respond on MII command\n", slave);
            return -1;
        }
    } while (ctrl_stat & 0x8000);

    return 0;
}

