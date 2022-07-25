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
 * If not, see <www.gnu.org/licenses/>.
 */

#include "config.h"

#include "libethercat/mii.h"
#include "libethercat/ec.h"
#include "libethercat/error_codes.h"

#include <assert.h>
#include <string.h>

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
        uint8_t phy_adr, uint16_t phy_reg, uint16_t *data) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(data != NULL);

    int ret = EC_OK;
    uint16_t wkc;
    uint16_t phy_adr_reg = phy_adr | ((uint16_t)phy_reg << 8);
    uint16_t ctrl_stat = 0;

    // write phy address amd regoster
    check_ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_MII_PHY_ADR, (uint8_t *)&phy_adr_reg, sizeof(phy_adr_reg), &wkc);

    // execute read command
    check_ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_MII_CTRLSTAT, (uint8_t *)&ctrl_stat, sizeof(ctrl_stat), &wkc);
    ctrl_stat &= ~(uint16_t)0x0300u;
    ctrl_stat |= (uint16_t)0x0100u;
    check_ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_MII_CTRLSTAT, (uint8_t *)&ctrl_stat, sizeof(ctrl_stat), &wkc);

    // set up timeout 100 ms
    ec_timer_t timeout;
    ec_timer_init(&timeout, 10000000000);

    do {
        check_ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_MII_CTRLSTAT, (uint8_t *)&ctrl_stat, sizeof(ctrl_stat), &wkc);

        if (ec_timer_expired(&timeout) == 1) {
            ec_log(10, __func__, "slave %2d did not respond on MII command\n", slave);
            ret = EC_ERROR_TIMEOUT;
        }
    } while (((ctrl_stat & 0x8000u) != 0u) && (ret == EC_OK));

        
    check_ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_MII_PHY_DATA, (uint8_t *)data, sizeof(*data), &wkc);
    return ret;
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
        uint8_t phy_adr, uint16_t phy_reg, uint16_t *data) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(data != NULL);

    int ret = EC_OK;
    uint16_t wkc;
    uint16_t phy_adr_reg = phy_adr | ((uint16_t)phy_reg << 8);
    uint16_t ctrl_stat = 0;

    // write phy address, register and data
    check_ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_MII_PHY_ADR, (uint8_t *)&phy_adr_reg, sizeof(phy_adr_reg), &wkc);      
    check_ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_MII_PHY_DATA, (uint8_t *)data, sizeof(*data), &wkc);

    // execute write command
    check_ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_MII_CTRLSTAT, (uint8_t *)&ctrl_stat, sizeof(ctrl_stat), &wkc);
    ctrl_stat &= ~(uint16_t)0x0300u;
    ctrl_stat |= (uint16_t)0x0201u;
    check_ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_MII_CTRLSTAT, (uint8_t *)&ctrl_stat, sizeof(ctrl_stat), &wkc);

    // set up timeout 100 ms
    ec_timer_t timeout;
    ec_timer_init(&timeout, 100000000);

    do {
        check_ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_MII_CTRLSTAT, (uint8_t *)&ctrl_stat, sizeof(ctrl_stat), &wkc);

        if (ec_timer_expired(&timeout) == 1) {
            ec_log(10, __func__, "slave %2d did not respond on MII command\n", slave);
            ret = EC_ERROR_TIMEOUT;
        }
    } while (((ctrl_stat & 0x8000u) != 0u) && (ret == EC_OK));

    return ret;
}

