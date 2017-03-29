/**
 * \file eeprom.c
 *
 * \author Robert Burger <robert.burger@dlr.de>
 *
 * \date 24 Nov 2016
 *
 * \brief ethercat eeprom access fuctions
 *
 * These functions are used to ensure access to the EtherCAT
 * slaves EEPROM.
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

#include "libethercat/eeprom.h"
#include "libethercat/ec.h"

#include <string.h>

#define SII_REG(ac, adr, val)                                          \
    cnt = 100;                                                         \
    do { ec_fp##ac(pec, pec->slaves[slave].fixed_address, (adr),       \
                (uint8_t *)&(val), sizeof(val), &wkc);                 \
    } while (--cnt > 0 && wkc != 1);

//! set eeprom control to pdi
/*!
 * \param pec pointer to ethercat master
 * \param slave ethercat slave number
 * \return 0 on success
 */
int ec_eeprom_to_pdi(ec_t *pec, uint16_t slave) {
    uint16_t wkc, cnt = 10;
    uint8_t eepctl = 2;

    eepctl = 1; 
    SII_REG(wr, EC_REG_EEPCFG, eepctl);

    return 0;
}

//! set eeprom control to ec
/*!
 * \param pec pointer to ethercat master
 * \param slave ethercat slave number
 * \return 0 on success
 */
int ec_eeprom_to_ec(struct ec *pec, uint16_t slave) {
    uint16_t wkc, cnt = 10;
    uint8_t eepctl = 2;

    SII_REG(rd, EC_REG_EEPCFG, eepctl);
    if (cnt == 0) {
        ec_log(10, __func__, "slave %2d: unable to get eeprom "
                "config/control\n", slave);
        return -1;
    }

    if (((eepctl & 0x0001) == 0x0000) && ((eepctl & 0x0100) == 0x0000))
        return 0; // ECAT has alread EEPROM control

    // ECAT assigns EEPROM interface to ECAT by writing 0x0500[0]=0
    eepctl = 0;
    SII_REG(wr, EC_REG_EEPCFG, eepctl);
    if (cnt == 0) {
        ec_log(10, __func__, "slave %2d did not accept assigning EEPROM "
                "to PDI\n", slave);
        return -1;
    }

    SII_REG(rd, EC_REG_EEPCFG, eepctl);
    if (cnt == 0) {
        ec_log(10, __func__, "slave %2d: unable to get eeprom "
                "config/control\n", slave);
        return -1;
    }

    if (((eepctl & 0x0001) == 0x0000) && ((eepctl & 0x0100) == 0x0000))
        return 0; // ECAT has EEPROM control
    
    return -1;
}

//! read 32-bit word of eeprom
/*!
 * \param pec pointer to ethercat master
 * \param slave ethercat slave number
 * \param eepadr address in eeprom
 * \param returns data value
 * \return 0 on success
 */
int ec_eepromread(ec_t *pec, uint16_t slave, uint32_t eepadr, uint32_t *data) {
    ec_eeprom_to_ec(pec, slave);
    
    int ret = 0, retry_cnt = 100;
    uint16_t wkc = 0, eepcsr = 0x0100; // read access
   
    do {
        eepcsr = 0;
        ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                (uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);
        if (--retry_cnt == 0) {
            ec_log(10, "EEPROM_READ", "reading eepctl failed, wkc %d\n", wkc);
            ret = -1;
            goto func_exit;
        }
    } while (eepcsr & 0x0100);

    ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_EEPADR,
            (uint8_t *)&eepadr, sizeof(eepadr), &wkc);
    if (wkc != 1) {
        ec_log(10, "EEPROM_READ", "writing eepadr failed\n");
        ret = -1;
        goto func_exit;
    }

    eepcsr = 0x0100;
    ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
            (uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);
    if (wkc != 1) {
        ec_log(10, "EEPROM_READ", "wirting eepctl failed\n");
        ret = -1;
        goto func_exit;
    }

    // 7. Wait until the Busy bit of the EEPROM Status register is cleared
    retry_cnt = 100;
    do {
        eepcsr = 0;
        ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                (uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);
        if (--retry_cnt == 0) {
            ec_log(10, "EEPROM_WRITE", "reading eepctl failed, wkc %d\n", wkc);
            ret = -1;
            goto func_exit;
        }
    } while (wkc == 0 || eepcsr & 0x8000);

    *data = 0;
    ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_EEPDAT,
            (uint8_t *)data, sizeof(*data), &wkc);
    if (wkc != 1) {
        ec_log(10, "EEPROM_READ", "reading data failed\n");
        ret = -1;
        goto func_exit;
    }
    
    // 8. Check the Error bits of the EEPROM Status register. The Error bits 
    // are cleared by clearing the command register. Retry command 
    // (back to step 5) if EEPROM acknowledge was missing. If necessary, 
    // wait some time before retrying to allow slow EEPROMs to store the data 
    // internally
    if (eepcsr & 0x0100)  
        ec_log(10, "EEPROM_WRITE", "write in progress\n");
    if (eepcsr & 0x4000)
        ec_log(10, "EEPROM_WRITE", "error write enable\n");
    if (eepcsr & 0x2000)
        ret = -1;
    if (eepcsr & 0x0800)
        ec_log(10, "EEPROM_WRITE", 
                "checksum error at in ESC configuration area\n");

func_exit:
    ec_eeprom_to_pdi(pec, slave);

    return ret;
}

//! write 32-bit word of eeprom
/*!
 * \param pec pointer to ethercat master
 * \param slave ethercat slave number
 * \param eepadr address in eeprom
 * \param returns data value
 * \return 0 on success
 */
int ec_eepromwrite(ec_t *pec, uint16_t slave, uint32_t eepadr, 
        uint16_t *data) {
    ec_eeprom_to_ec(pec, slave);
    
    int ret = 0, retry_cnt = 100;
    uint16_t wkc = 0, eepcsr = 0x0100; // write access

    // 1. Check if the Busy bit of the EEPROM Status register is 
    // cleared (0x0502[15]=0) and the EEPROM interface is not busy, 
    // otherwise wait until the EEPROM interface is not busy anymore.
    retry_cnt = 100;
    do {
        eepcsr = 0;
        ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                (uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);
        if (--retry_cnt == 0) {
            ec_log(10, "EEPROM_WRITE", "waiting for eeprom !busy failed, "
                    "wkc %d\n", wkc);
            ret = -1;
            goto func_exit;
        }
    } while ((wkc == 0) || ((eepcsr & 0x8000) != 0x0000));

    // 2. Check if the Error bits of the EEPROM Status register are 
    // cleared. If not, write “000” to the command register 
    // (register 0x0502 bits [10:8]).
    while (wkc == 0 || eepcsr & 0x6000) { 
        // we ignore crc errors on write .... 0x6800)
        // error bits set, clear first
        eepcsr = 0x0000;
        do {
            ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                    (uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);
        } while (wkc == 0);
        
        ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                (uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);
    }

    // 3. Write EEPROM word address to EEPROM Address register
    retry_cnt = 100;
    do {
        ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_EEPADR,
                (uint8_t *)&eepadr, sizeof(eepadr), &wkc);

        if (--retry_cnt == 0) {
            ec_log(10, "EEPROM_WRITE", "writing eepadr failed, wkc %d\n", wkc);
            ret = -1;
            goto func_exit;
        }
    } while (wkc == 0);

    // 4. Write command only: put write data into EEPROM Data register 
    // (1 word/2 byte only).
    retry_cnt = 100;
    do {
        ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_EEPDAT,
                (uint8_t *)data, sizeof(*data), &wkc);

        if (--retry_cnt == 0) {
            ec_log(10, "EEPROM_WRITE", "writing data failed\n");
            ret = -1;
            goto func_exit;
        }
    } while (wkc == 0);

    // 5. Issue command by writing to Control register.  
    // b) For a write command, write 1 into Write Enable bit 0x0502[0]
    // and 010 into Command Register 0x0502[10:8]. Both bits have to be 
    // written in one frame. The Write enable bit realizes a write protection 
    // mechanism. It is valid for subsequent EEPROM commands issued in the 
    // same frame and self-clearing afterwards. The Write enable bit needs 
    // not to be written from PDI if it controls the EEPROM interface.
    eepcsr = 0x0201;
    retry_cnt = 100;
    do {
        ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                (uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);

        if (--retry_cnt == 0) {
            ec_log(10, "EEPROM_WRITE", "wirting eepctl failed\n");
            ret = -1;
            goto func_exit;
        }
    } while (wkc == 0);

    // 6. The command is executed after the EOF if the EtherCAT frame had 
    // no errors. With PDI control, the command is executed immediately.

    // 7. Wait until the Busy bit of the EEPROM Status register is cleared
    retry_cnt = 100;
    do {
        eepcsr = 0;
        ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                (uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);
        if (--retry_cnt == 0) {
            ec_log(10, "EEPROM_WRITE", "reading eepctl failed, wkc %d\n", wkc);
            ret = -1;
            goto func_exit;
        }
    } while (wkc == 0 || eepcsr & 0x8000);

    // 8. Check the Error bits of the EEPROM Status register. The Error bits 
    // are cleared by clearing the command register. Retry command
    // (back to step 5) if EEPROM acknowledge was missing. If necessary, 
    // wait some time before retrying to allow slow EEPROMs to store the 
    // data internally
    if (eepcsr & 0x0100)  
        ec_log(10, "EEPROM_WRITE", "write in progress\n");
    if (eepcsr & 0x4000)
        ec_log(10, "EEPROM_WRITE", "error write enable\n");
    if (eepcsr & 0x2000)
        ret = -1;
    if (eepcsr & 0x0800)
        ec_log(10, "EEPROM_WRITE", 
                "checksum error at in ESC configuration area\n");

func_exit:
    ec_eeprom_to_pdi(pec, slave);

    return ret;
}

//! read a burst of eeprom
/*!
 * \param pec pointer to ethercat master
 * \param slave ethercat slave number
 * \param eepadr address in eeprom
 * \param buf return buffer
 * \param buflen length in bytes to return
 * \return 0 on success
 */
int ec_eepromread_len(ec_t *pec, uint16_t slave, uint32_t eepadr, 
        uint8_t *buf, size_t buflen) {
    unsigned offset = 0, i, ret;

    while (offset < buflen) {
        uint32_t val;

        do {
            ret = ec_eepromread(pec, slave, eepadr+(offset/2), &val);
            if (ret == -2)
                return ret;
        } while (ret != 0);

        for (i = 0; (offset < buflen) && (i < 4); ++i, ++offset)
            buf[offset] = ((uint8_t *)&val)[i];
    }

    return 0;
};

//! write a burst of eeprom
/*!
 * \param pec pointer to ethercat master
 * \param slave ethercat slave number
 * \param eepadr address in eeprom
 * \param buf return buffer
 * \param buflen length in bytes to return
 * \return 0 on success
 */
int ec_eepromwrite_len(ec_t *pec, uint16_t slave, uint32_t eepadr, 
        uint8_t *buf, size_t buflen) {
    unsigned offset = 0, i, ret;

    while (offset < buflen) {
        uint16_t val;
        for (i = 0; (offset < buflen/2) && (i < 2); ++i)
            ((uint8_t *)&val)[i] = buf[(offset*2)+i];
                
        ec_log(100, __func__, "slave %2d, writing adr %d\n", 
                        slave, eepadr+offset);

        do {
            ret = ec_eepromwrite(pec, slave, eepadr+offset, &val);
        } while (ret != 0);

        offset+=1;
    }

    return 0;
};

//! read out whole eeprom and categories
/*!
 * \param pec pointer to ethercat master
 * \param slave ethercat slave number
 */
void ec_eeprom_dump(ec_t *pec, uint16_t slave) {
    int cat_offset = EC_EEPROM_ADR_CAT_OFFSET;
    uint16_t /*size,*/ cat_len, cat_type = 0;
    uint32_t value32 = 0;
    ec_slave_t *slv = &pec->slaves[slave];

    if (slv->eeprom.read_eeprom == 1)
        return;

#define eeprom(adr, mem) \
    ec_eepromread_len(pec, slave, (adr), (uint8_t *)&(mem), sizeof(mem));
#define eeprom_log(...) \
    if (pec->eeprom_log) ec_log(__VA_ARGS__)

    // read soem eeprom values
    eeprom(EC_EEPROM_ADR_VENDOR_ID,
            slv->eeprom.vendor_id);
    eeprom(EC_EEPROM_ADR_PRODUCT_CODE,
            slv->eeprom.product_code);
    eeprom(EC_EEPROM_ADR_MBX_SUPPORTED,
            slv->eeprom.mbx_supported);
    eeprom(EC_EEPROM_ADR_SIZE,
            value32);
    eeprom(EC_EEPROM_ADR_STD_MBX_RECV_OFF,
            slv->eeprom.mbx_receive_offset);
    eeprom(EC_EEPROM_ADR_STD_MBX_RECV_SIZE,
            slv->eeprom.mbx_receive_size);
    eeprom(EC_EEPROM_ADR_STD_MBX_SEND_OFF,
            slv->eeprom.mbx_send_offset);
    eeprom(EC_EEPROM_ADR_STD_MBX_SEND_SIZE,
            slv->eeprom.mbx_send_size);
    eeprom(EC_EEPROM_ADR_BOOT_MBX_RECV_OFF,  
            slv->eeprom.boot_mbx_receive_offset);
    eeprom(EC_EEPROM_ADR_BOOT_MBX_RECV_SIZE, 
            slv->eeprom.boot_mbx_receive_size);
    eeprom(EC_EEPROM_ADR_BOOT_MBX_SEND_OFF,
            slv->eeprom.boot_mbx_send_offset);
    eeprom(EC_EEPROM_ADR_BOOT_MBX_SEND_SIZE,
            slv->eeprom.boot_mbx_send_size);

    //size = value32 & 0x0000FFFF;

    while (cat_type != EC_EEPROM_CAT_END) {
        int ret = eeprom(cat_offset, value32);
        if (ret != 0)
            break;

        cat_type = (value32 & 0x0000FFFF);
        cat_len  = (value32 & 0xFFFF0000) >> 16;

        switch (cat_type) {
            default: 
            case EC_EEPROM_CAT_END:
            case EC_EEPROM_CAT_NOP:
                break;
            case EC_EEPROM_CAT_STRINGS: {
                eeprom_log(100, "EEPROM_STRINGS", "slave %2d: cat_len %d\n", 
                        slave, cat_len);
                
                uint8_t *buf = malloc(cat_len*2 + 1);
                buf[cat_len*2] = 0;
                ec_eepromread_len(pec, slave, cat_offset+2, buf, cat_len*2);

                int local_offset = 0, i;
                slv->eeprom.strings_cnt = buf[local_offset++];

                eeprom_log(100, "EEPROM_STRINGS", "slave %2d: stored strings "
                        "%d\n", slave, slv->eeprom.strings_cnt);

                if (!slv->eeprom.strings_cnt) {
                    free(buf);
                    break;
                }

                slv->eeprom.strings = (char **)malloc(sizeof(char *) * 
                        slv->eeprom.strings_cnt);

                for (i = 0; i < slv->eeprom.strings_cnt; ++i) {
                    uint8_t string_len = buf[local_offset++];

                    slv->eeprom.strings[i] = malloc(sizeof(char) * 
                            (string_len + 1));
                    strncpy(slv->eeprom.strings[i], 
                            (char *)&buf[local_offset], string_len);
                    local_offset+=string_len;

                    slv->eeprom.strings[i][string_len] = '\0';
                    
                    eeprom_log(100, "EEPROM_STRINGS", 
                            "          string %2d, length %2d : %s\n", 
                            i, string_len, slv->eeprom.strings[i]);
                    if (local_offset > cat_len*2) {
                        eeprom_log(5, "EEPROM_STRINGS", 
                                "          something wrong in eeprom "
                                "string section\n");
                        break;
                    }
                }

                free(buf);
                break;
            }
            case EC_EEPROM_CAT_DATATYPES:
                eeprom_log(100, "EEPROM_DATATYPES", "slave %2d:\n", slave);

                break;
            case EC_EEPROM_CAT_GENERAL: {
                eeprom_log(100, "EEPROM_GENERAL", "slave %2d:\n", slave);

                eeprom(cat_offset+2, slv->eeprom.general);

                eeprom_log(100, "EEPROM_GENERAL", 
                        "          group_idx %d, img_idx %d, "
                        "order_idx %d, name_idx %d\n", 
                        slave, slv->eeprom.general.group_idx,
                        slv->eeprom.general.img_idx,
                        slv->eeprom.general.order_idx,
                        slv->eeprom.general.name_idx);
                break;
            }
            case EC_EEPROM_CAT_FMMU: {
                eeprom_log(100, "EEPROM_FMMU", "slave %2d: entries %d\n", 
                        slave, cat_len);

                // skip cat type and len
                int local_offset = cat_offset + 2;
                unsigned i, fmmu_idx = 0;
                while (local_offset < (cat_offset + cat_len + 2)) {
                    eeprom(local_offset, value32);
                    uint8_t *tmp = (uint8_t *)&value32;
                    for (i = 0; i < 4 && i < (cat_len*2); ++i, ++fmmu_idx)
                        if ((fmmu_idx < slv->fmmu_ch) && 
                                (tmp[i] >= 1) && (tmp[i] <= 3)) {
                            slv->fmmu[fmmu_idx].type = tmp[i];
                
                            eeprom_log(100, "EEPROM_FMMU", 
                                    "          fmmu%d, type %d\n", 
                                    fmmu_idx, tmp[i]);
                        }

                    local_offset += 2;
                }
                break;
            }
            case EC_EEPROM_CAT_SM: {
                eeprom_log(100, "EEPROM_SM", "slave %2d: entries %d\n", 
                        slave, cat_len/(sizeof(ec_eeprom_cat_sm_t)/2));

                // skip cat type and len
                int j = 0, local_offset = cat_offset + 2;
                slv->eeprom.sms_cnt = cat_len/(sizeof(ec_eeprom_cat_sm_t)/2);

                if (!slv->eeprom.sms_cnt)
                    break;

                // alloc sms
                slv->eeprom.sms = (ec_eeprom_cat_sm_t *)malloc(
                        sizeof(ec_eeprom_cat_sm_t) * slv->eeprom.sms_cnt);

                // reallocate if we have more sm that previously declared
                if ((cat_len/(sizeof(ec_eeprom_cat_sm_t)/2)) > slv->sm_ch) {
                    if (slv->sm)
                        free(slv->sm);

                    slv->sm_ch = cat_len/(sizeof(ec_eeprom_cat_sm_t)/2);
                    slv->sm = (ec_slave_sm_t *)malloc(slv->sm_ch * 
                            sizeof(ec_slave_sm_t));
                    memset(slv->sm, 0, slv->sm_ch * sizeof(ec_slave_sm_t));
                }

                while (local_offset < (cat_offset + cat_len + 2)) {
                    eeprom(local_offset, slv->eeprom.sms[j]);
                    local_offset += sizeof(ec_eeprom_cat_sm_t) / 2;

                    if (slv->sm[j].adr == 0) {
                        slv->sm[j].adr = slv->eeprom.sms[j].adr;
                        slv->sm[j].len = slv->eeprom.sms[j].len;
                        slv->sm[j].flags = (slv->eeprom.sms[j].activate << 16)
                            | slv->eeprom.sms[j].ctrl_reg;

                        eeprom_log(100, "EEPROM_SM", 
                                "          sm%d adr 0x%X, len %d, flags "
                                "0x%X\n", j, slv->sm[j].adr, slv->sm[j].len, 
                                slv->sm[j].flags);
                    } else {
                        eeprom_log(100, "EEPROM_SM", "          sm%d adr "
                                "0x%X, len %d, flags 0x%X\n", j, 
                                slv->eeprom.sms[j].adr, slv->eeprom.sms[j].len,
                                (slv->eeprom.sms[j].activate << 16) | 
                                slv->eeprom.sms[j].ctrl_reg);
                                
                        eeprom_log(100, "EEPROM_SM", 
                                "          sm%d already set by user\n", j);
                    }

                    j++;
                }
                break;
            }
            case EC_EEPROM_CAT_TXPDO: {
                eeprom_log(100, "EEPROM_TXPDO", "slave %2d:\n", slave);

                // skip cat type and len
                int j = 0, local_offset = cat_offset + 2;
                if (!cat_len)
                    break;

                // freeing tailq first
                ec_eeprom_cat_pdo_t *pdo;
                while ((pdo = TAILQ_FIRST(&slv->eeprom.txpdos)) != NULL) {
                    TAILQ_REMOVE(&slv->eeprom.txpdos, pdo, qh);
                    free(pdo);
                }

                // read pdo
                pdo = malloc(sizeof(ec_eeprom_cat_pdo_t));
                ec_eepromread_len(pec, slave, local_offset, 
                        (uint8_t *)pdo, EC_EEPROM_CAT_PDO_LEN);
                local_offset += EC_EEPROM_CAT_PDO_LEN / 2;
                        
                eeprom_log(100, "EEPROM_TXPDO", "          0x%04X\n",
                        pdo->pdo_index);
               
                if (pdo->n_entry) {
                    // alloc entries
                    pdo->entries = malloc(pdo->n_entry * 
                            sizeof(ec_eeprom_cat_pdo_entry_t));

                    for (j = 0; j < pdo->n_entry; ++j) {
                        ec_eeprom_cat_pdo_entry_t *entry = &pdo->entries[j];
                        ec_eepromread_len(pec, slave, local_offset,
                                (uint8_t *)entry, 
                                sizeof(ec_eeprom_cat_pdo_entry_t));

                        local_offset += sizeof(ec_eeprom_cat_pdo_entry_t) / 2;
                
                        eeprom_log(100, "EEPROM_TXPDO", 
                                "          0x%04X:%2d -> 0x%04X\n",
                                pdo->pdo_index, j, entry->entry_index);
                    }
                }

                TAILQ_INSERT_TAIL(&slv->eeprom.txpdos, pdo, qh);
                break;
            }
            case EC_EEPROM_CAT_RXPDO: {
                eeprom_log(100, "EEPROM_RXPDO", "slave %2d:\n", slave);

                // skip cat type and len
                int j = 0, local_offset = cat_offset + 2;
                if (!cat_len)
                    break;

                // freeing tailq first
                ec_eeprom_cat_pdo_t *pdo;
                while ((pdo = TAILQ_FIRST(&slv->eeprom.rxpdos)) != NULL) {
                    TAILQ_REMOVE(&slv->eeprom.rxpdos, pdo, qh);
                    free(pdo);
                }

                // read pdo
                pdo = malloc(sizeof(ec_eeprom_cat_pdo_t));
                ec_eepromread_len(pec, slave, local_offset, 
                        (uint8_t *)pdo, EC_EEPROM_CAT_PDO_LEN);
                local_offset += EC_EEPROM_CAT_PDO_LEN / 2;
                        
                eeprom_log(100, "EEPROM_RXPDO", "          0x%04X\n",
                        pdo->pdo_index);
               
                if (pdo->n_entry) {
                    // alloc entries
                    pdo->entries = malloc(pdo->n_entry * 
                            sizeof(ec_eeprom_cat_pdo_entry_t));

                    for (j = 0; j < pdo->n_entry; ++j) {
                        ec_eeprom_cat_pdo_entry_t *entry = &pdo->entries[j];
                        ec_eepromread_len(pec, slave, local_offset,
                                (uint8_t *)entry, 
                                sizeof(ec_eeprom_cat_pdo_entry_t));

                        local_offset += sizeof(ec_eeprom_cat_pdo_entry_t) / 2;
                
                        eeprom_log(100, "EEPROM_TXPDO", 
                                "          0x%04X:%2d -> 0x%04X\n",
                                pdo->pdo_index, j, entry->entry_index);
                    }
                }

                TAILQ_INSERT_TAIL(&slv->eeprom.rxpdos, pdo, qh);
                break;
            }
            case EC_EEPROM_CAT_DC: {
                int j = 0, local_offset = cat_offset + 2;

                eeprom_log(100, "EEPROM_DC", "slave %2d:\n", slave);
                
                // freeing existing dcs ...
                if (slv->eeprom.dcs) {
                    free(slv->eeprom.dcs);
                    slv->eeprom.dcs = NULL;
                    slv->eeprom.dcs_cnt = 0;
                }
                
                // allocating new dcs
                slv->eeprom.dcs_cnt = cat_len/(EC_EEPROM_CAT_DC_LEN/2);
                slv->eeprom.dcs = malloc(EC_EEPROM_CAT_DC_LEN * 
                        slv->eeprom.dcs_cnt);

                for (j = 0; j < slv->eeprom.dcs_cnt; ++j) {
                    ec_eeprom_cat_dc_t *dc = &slv->eeprom.dcs[j];
                    ec_eepromread_len(pec, slave, local_offset,
                            (uint8_t *)dc, EC_EEPROM_CAT_DC_LEN);
                    local_offset += EC_EEPROM_CAT_DC_LEN/2;
                
                    eeprom_log(100, "EEPROM_DC", "          cycle_time_0 %d, "
                            "shift_time_0 %d, shift_time_1 %d, "
                            "sync_0_cycle_factor %d, sync_1_cycle_factor %d, "
                            "assign_active %d\n", 
                            dc->cycle_time_0, dc->shift_time_0, 
                            dc->shift_time_1, dc->sync_0_cycle_factor, 
                            dc->sync_1_cycle_factor, dc->assign_active);                   
                }

                break;
            }
        }

        cat_offset += cat_len + 2; 
    }
    
    slv->eeprom.read_eeprom = 1;
}

