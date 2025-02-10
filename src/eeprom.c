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

#include "libethercat/eeprom.h"
#include "libethercat/ec.h"
#include "libethercat/error_codes.h"

#include <assert.h>
#include <string.h>
#include <inttypes.h>

// cppcheck-suppress misra-c2012-20.10
#define SII_REG(ac, adr, val)                                          \
    cnt = 100u;                                                        \
    do { ret = ec_fp##ac(pec, pec->slaves[slave].fixed_address, (adr), \
                (osal_uint8_t *)&(val), sizeof(val), &wkc);                 \
    } while ((--cnt > 0u) && (wkc != 1u) && (ret == EC_OK));

// set eeprom control to pdi
int ec_eeprom_to_pdi(ec_t *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    int ret = EC_OK;
    osal_uint16_t wkc;
    osal_uint16_t cnt = 10u;
    osal_uint8_t eepcfg = 2u;

    eepcfg = 1; 
    SII_REG(wr, EC_REG_EEPCFG, eepcfg);
    if ((ret != EC_OK) || (cnt == 0u)) {
        ret = EC_ERROR_EEPROM_CONTROL_TO_PDI;
    }

    return ret;
}

// set eeprom control to ec
int ec_eeprom_to_ec(struct ec *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    int ret = EC_OK;
    osal_uint16_t wkc;
    osal_uint16_t cnt = 10;
    osal_uint16_t eepcfgstat = 2;
    
    SII_REG(rd, EC_REG_EEPCFG, eepcfgstat);
    if ((ret != EC_OK) || (cnt == 0u)) {
        ec_log(1, "EEPROM_TO_EC", "slave %2d: unable to get eeprom config/control\n", slave);
        ret = EC_ERROR_EEPROM_CONTROL_TO_EC;
    }

    if (ret == EC_OK) {
        if (((eepcfgstat & 0x0001u) == 0x0000u) && ((eepcfgstat & 0x0100u) == 0x0000u)) {
        } else {
            // ECAT assigns EEPROM interface to ECAT by writing 0x0500[0]=0
            osal_uint8_t eepcfg = 0;
            SII_REG(wr, EC_REG_EEPCFG, eepcfg);
            if ((ret != EC_OK) || (cnt == 0u)) {
                ec_log(1, "EEPROM_TO_EC", "slave %2d did not accept assigning EEPROM to PDI\n", slave);
                ret = EC_ERROR_EEPROM_CONTROL_TO_EC;
            }
        }
    }

    if (ret == EC_OK) {
        int retry_cnt = 100;

        do {
            SII_REG(rd, EC_REG_EEPCFG, eepcfgstat);
            if ((ret != EC_OK) || (cnt == 0u)) {
                ec_log(1, "EEPROM_TO_EC", "slave %2d: unable to get eeprom config/control\n", slave);
                ret = EC_ERROR_EEPROM_CONTROL_TO_EC;
            } else if (((eepcfgstat & 0x0001u) == 0x0000u) && ((eepcfgstat & 0x0100u) == 0x0000u)) {
                // ECAT has EEPROM control
                ret = EC_OK;
                break;
            }

            osal_sleep(1000000);

        } while (--retry_cnt > 0);

        if (retry_cnt <= 0) {
            ec_log(10, "EEPROM_TO_EC", "slave %2d: failed setting eeprom to EtherCAT: eepcfgstat %04X\n", slave, eepcfgstat);
        }
    }

    return ret;
}

// read 32-bit word of eeprom
int ec_eepromread(ec_t *pec, osal_uint16_t slave, osal_uint32_t eepadr, osal_uint32_t *data) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(data != NULL);
   
    int ret = EC_OK;
    int retry_cnt = 100;
    osal_uint16_t wkc = 0;
    osal_uint16_t eepcsr = 0x0100; // read access
    
    ret = ec_eeprom_to_ec(pec, slave);
    
    // 1. Check if the Busy bit of the EEPROM Status register is 
    // cleared (0x0502[15]=0) and the EEPROM interface is not busy, 
    // otherwise wait until the EEPROM interface is not busy anymore.
    if (ret == EC_OK) {
        do {
            eepcsr = 0;
            ret = ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                    (osal_uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);

            retry_cnt--;
            if ((ret == EC_OK) && (retry_cnt == 0)) {
                ec_log(1, "EEPROM_READ", "reading eepctl failed, wkc %d\n", wkc);
                ret = EC_ERROR_EEPROM_READ_ERROR;
            }
        } while (((eepcsr & 0x0100u) != 0u) && (ret == EC_OK));
    } 

    if (ret == EC_OK) {
        ret = ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_EEPADR,
                (osal_uint8_t *)&eepadr, sizeof(eepadr), &wkc);

        if ((ret == EC_OK) && (wkc != 1u)) {
            ec_log(1, "EEPROM_READ", "writing eepadr failed\n");
            ret = EC_ERROR_EEPROM_READ_ERROR;
        }
    }

    if (ret == EC_OK) {
        eepcsr = 0x0100;
        ret = ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                (osal_uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);
        if ((ret == EC_OK) && (wkc != 1u)) {
            ec_log(1, "EEPROM_READ", "wirting eepctl failed\n");
            ret = EC_ERROR_EEPROM_READ_ERROR;
        }
    }

    // 7. Wait until the Busy bit of the EEPROM Status register is cleared
    if (ret == EC_OK) {
        retry_cnt = 100;
        do {
            eepcsr = 0;
            ret = ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                    (osal_uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);

            retry_cnt--;
            if ((ret == EC_OK) && (retry_cnt == 0)) {
                ec_log(1, "EEPROM_READ", "reading eepctl failed, wkc %d\n", wkc);
                ret = EC_ERROR_EEPROM_READ_ERROR;
            }
        } while (((wkc == 0u) || (eepcsr & 0x8000u)) && (ret == EC_OK));
    }

    if (ret == EC_OK) {
        *data = 0;
        ret = ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_EEPDAT,
                (osal_uint8_t *)data, sizeof(*data), &wkc);
        if ((ret == EC_OK) && (wkc != 1u)) {
            ec_log(1, "EEPROM_READ", "reading data failed\n");
            ret = EC_ERROR_EEPROM_READ_ERROR;
        }
    }
    
    // 8. Check the Error bits of the EEPROM Status register. The Error bits 
    // are cleared by clearing the command register. Retry command 
    // (back to step 5) if EEPROM acknowledge was missing. If necessary, 
    // wait some time before retrying to allow slow EEPROMs to store the data 
    // internally
    if (ret == EC_OK) {
        if ((eepcsr & 0x0100u) != 0u) {
            ec_log(10, "EEPROM_READ", "write in progress\n");
            ret = EC_ERROR_EEPROM_WRITE_IN_PROGRESS;
        } else if ((eepcsr & 0x4000u) != 0u) {
            ec_log(1, "EEPROM_READ", "error write enable\n");
            ret = EC_ERROR_EEPROM_WRITE_ENABLE;
        } else if ((eepcsr & 0x2000u) != 0u) {
            ret = EC_ERROR_EEPROM_READ_ERROR;
        } else if ((eepcsr & 0x0800u) != 0u) {
            ec_log(1, "EEPROM_READ", "checksum error at in ESC configuration area\n");
            ret = EC_ERROR_EEPROM_CHECKSUM;
        } else {}
    }

    if (ret == EC_OK) {
        ret = ec_eeprom_to_pdi(pec, slave);
    }

    return ret;
}

// write 32-bit word of eeprom
int ec_eepromwrite(ec_t *pec, osal_uint16_t slave, osal_uint32_t eepadr, osal_uint16_t *data) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(data != NULL);

    int ret = EC_OK;
    int retry_cnt = 100;
    osal_uint16_t wkc = 0;
    osal_uint16_t eepcsr = 0x0100; // write access
    
    ret = ec_eeprom_to_ec(pec, slave);
    
    // 1. Check if the Busy bit of the EEPROM Status register is 
    // cleared (0x0502[15]=0) and the EEPROM interface is not busy, 
    // otherwise wait until the EEPROM interface is not busy anymore.
    retry_cnt = 100;
    if (ret == EC_OK) {
        do {
            eepcsr = 0;
            ret = ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                    (osal_uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);

            retry_cnt--;
            if ((ret == EC_OK) && (retry_cnt == 0)) {
                ec_log(1, "EEPROM_WRITE", "waiting for eeprom !busy failed, "
                        "wkc %d\n", wkc);
                ret = EC_ERROR_EEPROM_WRITE_ERROR;
            }
        } while (((wkc == 0u) || ((eepcsr & 0x8000u) != 0x0000u)) && (ret == EC_OK));
    }

    // 2. Check if the Error bits of the EEPROM Status register are 
    // cleared. If not, write “000” to the command register 
    // (register 0x0502 bits [10:8]).
    if (ret == EC_OK) {
        while ((wkc == 0u) || ((eepcsr & 0x6000u) != 0u)) { 
            // we ignore crc errors on write .... 0x6800)
            // error bits set, clear first
            eepcsr = 0x0000u;
            do {
                ret = ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                        (osal_uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);
            } while ((ret == EC_OK) && (wkc == 0u));

            if (ret == EC_OK) {
                ret = ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                    (osal_uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);
            }
        }
    }

    // 3. Write EEPROM word address to EEPROM Address register
    retry_cnt = 100;
    if (ret == EC_OK) {
        do {
            ret = ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_EEPADR,
                    (osal_uint8_t *)&eepadr, sizeof(eepadr), &wkc);

            retry_cnt--;
            if ((ret == EC_OK) && (retry_cnt == 0)) {
                ec_log(1, "EEPROM_WRITE", "writing eepadr failed, wkc %d\n", wkc);
                ret = EC_ERROR_EEPROM_WRITE_IN_PROGRESS;
            }
        } while ((ret == EC_OK) && (wkc == 0u));
    }

    // 4. Write command only: put write data into EEPROM Data register 
    // (1 word/2 byte only).
    retry_cnt = 100;
    if (ret == EC_OK) {
        do {
            ret = ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_EEPDAT,
                    (osal_uint8_t *)data, sizeof(*data), &wkc);

            retry_cnt--;
            if ((ret == EC_OK) && (retry_cnt == 0)) {
                ec_log(1, "EEPROM_WRITE", "writing data failed\n");
                ret = EC_ERROR_EEPROM_WRITE_ERROR;
            }
        } while ((ret == EC_OK) && (wkc == 0u));
    }

    // 5. Issue command by writing to Control register.  
    // b) For a write command, write 1 into Write Enable bit 0x0502[0]
    // and 010 into Command Register 0x0502[10:8]. Both bits have to be 
    // written in one frame. The Write enable bit realizes a write protection 
    // mechanism. It is valid for subsequent EEPROM commands issued in the 
    // same frame and self-clearing afterwards. The Write enable bit needs 
    // not to be written from PDI if it controls the EEPROM interface.
    eepcsr = 0x0201;
    retry_cnt = 100;
    if (ret == EC_OK) {
        do {
            ret = ec_fpwr(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                    (osal_uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);

            retry_cnt--;
            if ((ret == EC_OK) && (retry_cnt == 0)) {
                ec_log(1, "EEPROM_WRITE", "wirting eepctl failed\n");
                ret = EC_ERROR_EEPROM_WRITE_ERROR;
            }
        } while ((ret == EC_OK) && (wkc == 0u));
    }

    // 6. The command is executed after the EOF if the EtherCAT frame had 
    // no errors. With PDI control, the command is executed immediately.

    // 7. Wait until the Busy bit of the EEPROM Status register is cleared
    if (ret == EC_OK) {
        retry_cnt = 100;
        do {
            eepcsr = 0;
            ret = ec_fprd(pec, pec->slaves[slave].fixed_address, EC_REG_EEPCTL,
                    (osal_uint8_t *)&eepcsr, sizeof(eepcsr), &wkc);

            retry_cnt--;
            if ((ret == EC_OK) && (retry_cnt == 0)) {
                ec_log(1, "EEPROM_WRITE", "reading eepctl failed, wkc %d\n", wkc);
                ret = EC_ERROR_EEPROM_WRITE_ERROR;
            }
        } while (((wkc == 0u) || ((eepcsr & 0x8000u) != 0u)) && (ret == EC_OK));
    }

    // 8. Check the Error bits of the EEPROM Status register. The Error bits 
    // are cleared by clearing the command register. Retry command
    // (back to step 5) if EEPROM acknowledge was missing. If necessary, 
    // wait some time before retrying to allow slow EEPROMs to store the 
    // data internally
    if (ret == EC_OK) {
        if ((eepcsr & 0x0100u) != 0u) { 
            ec_log(1, "EEPROM_WRITE", "write in progress\n");
            ret = EC_ERROR_EEPROM_WRITE_IN_PROGRESS;
        } else if ((eepcsr & 0x4000u) != 0u) {
            ec_log(1, "EEPROM_WRITE", "error write enable\n");
            ret = EC_ERROR_EEPROM_WRITE_ENABLE;
        } else if ((eepcsr & 0x2000u) != 0u) {
            ret = EC_ERROR_EEPROM_WRITE_ERROR;
        } else if ((eepcsr & 0x0800u) != 0u) {
            // ignore this on write
            //ec_log(1, "EEPROM_WRITE", 
            //        "checksum error at in ESC configuration area\n");
            //ret = EC_ERROR_EEPROM_CHECKSUM;
        } else {}
    }

    if (ret == EC_OK) {
        ret = ec_eeprom_to_pdi(pec, slave);
    }

    return ret;
}

// read a burst of eeprom
int ec_eepromread_len(ec_t *pec, osal_uint16_t slave, osal_uint32_t eepadr, 
        osal_uint8_t *buf, osal_size_t buflen) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(buf != NULL);

    osal_off_t offset = 0;
    int ret = EC_OK;
    
    while (offset < buflen) {
        osal_uint8_t val[4];
        int i;

        // cppcheck-suppress misra-c2012-11.3
        ret = ec_eepromread(pec, slave, eepadr+(offset/2u), (osal_uint32_t *)&val[0]);
        if (ret != EC_OK) {
            break;
        }

        i = 0;
        while ((offset < buflen) && (i < 4)) {
            buf[offset] = val[i];
            offset++;
            i++;
        }
    }

    return ret;
};

// write a burst of eeprom
int ec_eepromwrite_len(ec_t *pec, osal_uint16_t slave, osal_uint32_t eepadr, 
        const osal_uint8_t *buf, osal_size_t buflen) 
{
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);
    assert(buf != NULL);

    osal_off_t offset = 0;
    int i;
    int ret = EC_OK;

    while (offset < (buflen/2u)) {
        osal_uint8_t val[2];
        for (i = 0; i < 2; ++i) {
            val[i] = buf[(offset*2)+i];
        }
                
        ec_log(100, "EEPROM_WRITE", "slave %2d, writing adr %" PRIu64 " : 0x%02X%02X\n", 
                        slave, eepadr+offset, val[1], val[0]);

        do {
            ret = ec_eepromwrite(pec, slave, eepadr+offset, (osal_uint16_t *)&val);
        } while (ret != EC_OK);

        offset +=1u;
    }

    return ret;
};

// read out whole eeprom and categories
void ec_eeprom_dump(ec_t *pec, osal_uint16_t slave) {
    assert(pec != NULL);
    assert(slave < pec->slave_cnt);

    osal_off_t cat_offset = EC_EEPROM_ADR_CAT_OFFSET;
    osal_uint32_t value32 = 0;
    
    ec_slave_ptr(slv, pec, slave);

    if (slv->eeprom.read_eeprom == 0) {
        osal_uint16_t size;

#define ec_read_eeprom(adr, mem) \
        ec_eepromread_len(pec, slave, (adr), (osal_uint8_t *)&(mem), sizeof(mem));
#define do_eeprom_log(...) \
        if (pec->eeprom_log != 0) { ec_log(__VA_ARGS__); }

        // read soem eeprom values
        (void)ec_read_eeprom(EC_EEPROM_ADR_VENDOR_ID, slv->eeprom.vendor_id);
        (void)ec_read_eeprom(EC_EEPROM_ADR_PRODUCT_CODE, slv->eeprom.product_code);
        (void)ec_read_eeprom(EC_EEPROM_ADR_MBX_SUPPORTED, slv->eeprom.mbx_supported);
        (void)ec_read_eeprom(EC_EEPROM_ADR_SIZE, value32);
        (void)ec_read_eeprom(EC_EEPROM_ADR_STD_MBX_RECV_OFF, slv->eeprom.mbx_receive_offset);
        (void)ec_read_eeprom(EC_EEPROM_ADR_STD_MBX_RECV_SIZE, slv->eeprom.mbx_receive_size);
        (void)ec_read_eeprom(EC_EEPROM_ADR_STD_MBX_SEND_OFF, slv->eeprom.mbx_send_offset);
        (void)ec_read_eeprom(EC_EEPROM_ADR_STD_MBX_SEND_SIZE, slv->eeprom.mbx_send_size);
        (void)ec_read_eeprom(EC_EEPROM_ADR_BOOT_MBX_RECV_OFF, slv->eeprom.boot_mbx_receive_offset);
        (void)ec_read_eeprom(EC_EEPROM_ADR_BOOT_MBX_RECV_SIZE, slv->eeprom.boot_mbx_receive_size);
        (void)ec_read_eeprom(EC_EEPROM_ADR_BOOT_MBX_SEND_OFF, slv->eeprom.boot_mbx_send_offset);
        (void)ec_read_eeprom(EC_EEPROM_ADR_BOOT_MBX_SEND_SIZE, slv->eeprom.boot_mbx_send_size);

        slv->eeprom.read_eeprom = 1;

        size = (osal_uint16_t)(((value32 & 0x0000FFFFu) + 1u) * 125u); // convert kbit to byte
        if (size > 128u) {
            osal_uint16_t cat_type;
            osal_uint32_t free_pdo_index = 0;
            do {
                int ret = ec_read_eeprom(cat_offset, value32);
                if (ret != 0) {
                    break;
                }

                cat_type = (osal_uint16_t)(value32 & 0x0000FFFFu);
                osal_uint16_t cat_len  = (osal_uint16_t)((value32 & 0xFFFF0000u) >> 16u);

                switch (cat_type) {
                    default: 
                    case EC_EEPROM_CAT_END:
                    case EC_EEPROM_CAT_NOP:
                        break;
                    case EC_EEPROM_CAT_STRINGS: {
                        do_eeprom_log(10, "EEPROM_STRINGS", "slave %2d: cat_len %d\n", slave, cat_len);
                        static osal_uint8_t ec_eeprom_buf[4096];
                        (void)ec_eepromread_len(pec, slave, cat_offset+2, &ec_eeprom_buf[0], cat_len * 2u);

                        osal_uint32_t local_offset = 0;
                        osal_uint32_t i;
                        slv->eeprom.strings_cnt = ec_eeprom_buf[local_offset];
                        local_offset++;

                        do_eeprom_log(10, "EEPROM_STRINGS", "slave %2d: stored strings %d\n", slave, slv->eeprom.strings_cnt);
                        if (slv->eeprom.strings_cnt > LEC_MAX_EEPROM_CAT_STRINGS) {
                            do_eeprom_log(10, "EEPROM_STRINGS", "        : warning: can only store %" PRIu64 " strings\n", LEC_MAX_EEPROM_CAT_STRINGS);
                        }

                        if (!slv->eeprom.strings_cnt) {
                            break;
                        }

                        for (i = 0; i < slv->eeprom.strings_cnt; ++i) {
                            osal_uint8_t string_len = ec_eeprom_buf[local_offset];
                            osal_uint8_t read_string_len = 0u; //buf[local_offset];
                            local_offset++;

                            if (string_len > (LEC_MAX_STRING_LEN - 1u)) {
                                do_eeprom_log(10, "EEPROM_STRINGS", "        : warning string is %d bytes long, can only read %" PRIu64 " bytes.\n", 
                                        string_len, LEC_MAX_STRING_LEN - 1u);
                                read_string_len = LEC_MAX_STRING_LEN - 1u;
                            } else {
                                read_string_len = string_len;
                            }

                            if (i >= LEC_MAX_EEPROM_CAT_STRINGS) {
                                char tmp[65];
                                read_string_len = min(64, read_string_len);
                                (void)strncpy(&tmp[0], (osal_char_t *)&ec_eeprom_buf[local_offset], read_string_len);
                                local_offset += string_len;
                                tmp[read_string_len] = '\0';
                            
                                do_eeprom_log(10, "EEPROM_STRINGS", "        (-)  string %2d, length %2d : %s\n", i, string_len, tmp);
                            } else {
                                (void)strncpy(&slv->eeprom.strings[i][0], (osal_char_t *)&ec_eeprom_buf[local_offset], read_string_len);
                                local_offset += string_len;

                                slv->eeprom.strings[i][read_string_len] = '\0';

                                do_eeprom_log(10, "EEPROM_STRINGS", "        (S)  string %2d, length %2d : %s\n", i, string_len, slv->eeprom.strings[i]);
                            }
                            
                            if (local_offset > (cat_len * 2u)) {
                                do_eeprom_log(5, "EEPROM_STRINGS", "          something wrong in eeprom string section\n");
                                break;
                            }
                        }

                        break;
                    }
                    case EC_EEPROM_CAT_DATATYPES:
                        do_eeprom_log(10, "EEPROM_DATATYPES", "slave %2d:\n", slave);
                        break;
                    case EC_EEPROM_CAT_GENERAL: {
                        do_eeprom_log(10, "EEPROM_GENERAL", "slave %2d:\n", slave);

                        (void)ec_read_eeprom(cat_offset+2, slv->eeprom.general);

                        do_eeprom_log(10, "EEPROM_GENERAL", 
                                "          group_idx %d, img_idx %d, "
                                "order_idx %d, name_idx %d\n", 
                                slv->eeprom.general.group_idx,
                                slv->eeprom.general.img_idx,
                                slv->eeprom.general.order_idx,
                                slv->eeprom.general.name_idx);
                        break;
                    }
                    case EC_EEPROM_CAT_FMMU: {
                        do_eeprom_log(10, "EEPROM_FMMU", "slave %2d: entries %d\n", 
                                slave, cat_len);

                        // skip cat type and len
                        osal_uint32_t local_offset = cat_offset + 2u;
                        slv->eeprom.fmmus_cnt = cat_len * 2u;

                        if (!slv->eeprom.fmmus_cnt) {
                            break;
                        }

                        osal_uint32_t fmmu_idx = 0;
                        while (local_offset < (cat_offset + cat_len + 2u)) {
                            osal_uint32_t i;

                            (void)ec_read_eeprom(local_offset, value32);
                            osal_uint8_t tmp[4];
                            (void)memcpy(&tmp[0], (osal_uint8_t *)&value32, 4);

                            i = 0u;
                            while ((i < 4u) && (i < (cat_len * 2u))) {
                                if ((fmmu_idx < LEC_MAX_EEPROM_CAT_FMMU) && (fmmu_idx < slv->fmmu_ch) && (tmp[i] >= 1u) && (tmp[i] <= 3u)) 
                                {
                                    slv->fmmu[fmmu_idx].type = tmp[i];
                                    slv->eeprom.fmmus[fmmu_idx].type = tmp[i];

                                    do_eeprom_log(10, "EEPROM_FMMU", "          fmmu%d, type %d\n", fmmu_idx, tmp[i]);
                                }

                                i++;
                                fmmu_idx++;
                            }

                            local_offset += 2u;
                        }
                        break;
                    }
                    case EC_EEPROM_CAT_SM: {
                        do_eeprom_log(10, "EEPROM_SM", "slave %2d: entries %u\n", 
                                slave, (osal_uint32_t)(cat_len/(sizeof(ec_eeprom_cat_sm_t)/2u)));

                        // skip cat type and len
                        osal_uint32_t j = 0;
                        osal_off_t local_offset = cat_offset + 2;
                        slv->eeprom.sms_cnt = cat_len/(sizeof(ec_eeprom_cat_sm_t)/2u);

                        if (!slv->eeprom.sms_cnt) {
                            break;
                        }

                        // reallocate if we have more sm that previously declared
                        if ((cat_len/(sizeof(ec_eeprom_cat_sm_t) / 2u)) > slv->sm_ch) {
                            slv->sm_ch = cat_len/(sizeof(ec_eeprom_cat_sm_t)/2u);
                            (void)memset(slv->sm, 0, slv->sm_ch * sizeof(ec_slave_sm_t));
                        }

                        while (local_offset < (cat_offset + cat_len + 2u)) {
                            ec_eeprom_cat_sm_t tmp_sms;
                            (void)ec_read_eeprom(local_offset, tmp_sms);
                            local_offset += (osal_off_t)(sizeof(ec_eeprom_cat_sm_t) / 2u);

                            if (slv->sm[j].adr == 0u) {
                                slv->sm[j].adr = tmp_sms.adr;
                                slv->sm[j].len = tmp_sms.len;
                                slv->sm[j].enable_sm = tmp_sms.activate;
                                slv->sm[j].control_register = tmp_sms.ctrl_reg;

                                do_eeprom_log(10, "EEPROM_SM", 
                                        "          sm%d adr 0x%X, len %d, enable_sm "
                                        "0x%X, control_register 0x%X\n", j, slv->sm[j].adr, slv->sm[j].len, 
                                        slv->sm[j].enable_sm, slv->sm[j].control_register);
                            } else {
                                do_eeprom_log(10, "EEPROM_SM", "          sm%d adr "
                                        "0x%X, len %d, flags 0x%X\n", j, 
                                        tmp_sms.adr, tmp_sms.len, (tmp_sms.activate << 16) | tmp_sms.ctrl_reg);

                                do_eeprom_log(10, "EEPROM_SM", 
                                        "          sm%d already set by user\n", j);
                            }

                            if (j < LEC_MAX_EEPROM_CAT_SM) {
                                memcpy(&slv->eeprom.sms[j], &tmp_sms, sizeof(ec_eeprom_cat_sm_t));
                            }

                            j++;
                        }
                        break;
                    }
                    case EC_EEPROM_CAT_TXPDO: {
                        do_eeprom_log(100, "EEPROM_TXPDO", "slave %2d:\n", slave);

                        // skip cat type and len
                        osal_uint32_t j = 0;
                        osal_size_t local_offset = cat_offset + 2u;
                        if (!cat_len) {
                            break;
                        }

                        // freeing tailq first
                        ec_eeprom_cat_pdo_t *pdo = TAILQ_FIRST(&slv->eeprom.txpdos);
                        while (pdo != NULL) {
                            TAILQ_REMOVE(&slv->eeprom.txpdos, pdo, qh);
                            pdo = TAILQ_FIRST(&slv->eeprom.txpdos);
                        }

                        do {
                            // read pdo
                            ec_eeprom_cat_pdo_t tmp_pdo;

                            (void)memset((osal_uint8_t *)&tmp_pdo, 0, sizeof(ec_eeprom_cat_pdo_t));
                            (void)ec_eepromread_len(pec, slave, local_offset, 
                                    (osal_uint8_t *)&tmp_pdo, EC_EEPROM_CAT_PDO_LEN);
                            local_offset += (osal_size_t)(EC_EEPROM_CAT_PDO_LEN / 2u);

                            do_eeprom_log(10, "EEPROM_TXPDO", "          0x%04X, entries %d\n", tmp_pdo.pdo_index, tmp_pdo.n_entry);

                            if (tmp_pdo.n_entry > 0u) {
                                (void)memset(&tmp_pdo.entries[0], 0, sizeof(ec_eeprom_cat_pdo_entry_t) * LEC_MAX_EEPROM_CAT_PDO_ENTRIES);

                                for (j = 0; j < tmp_pdo.n_entry; ++j) {
                                    ec_eeprom_cat_pdo_entry_t entry;
                                    (void)ec_eepromread_len(pec, slave, local_offset,
                                            (osal_uint8_t *)&entry, 
                                            sizeof(ec_eeprom_cat_pdo_entry_t));

                                    local_offset += sizeof(ec_eeprom_cat_pdo_entry_t) / 2u;

                                    if (j >= LEC_MAX_EEPROM_CAT_PDO_ENTRIES) {
                                        do_eeprom_log(10, "EEPROM_TXPDO", 
                                                "          0x%04X:%2d -> 0x%04X (not stored)\n",
                                                tmp_pdo.pdo_index, j, entry.entry_index);
                                    } else {
                                        tmp_pdo.entries[j] = entry;

                                        do_eeprom_log(10, "EEPROM_TXPDO", 
                                                "          0x%04X:%2d -> 0x%04X\n",
                                                tmp_pdo.pdo_index, j, entry.entry_index);
                                    }
                                }
                            }

                            if (free_pdo_index < LEC_MAX_EEPROM_CAT_PDO_ENTRIES) {
                                pdo = &slv->eeprom.free_pdos[free_pdo_index];
                                free_pdo_index++;
                                memcpy(pdo, &tmp_pdo, sizeof(ec_eeprom_cat_pdo_t));
                                TAILQ_INSERT_TAIL(&slv->eeprom.txpdos, pdo, qh);
                            }
                        } while (local_offset < (cat_offset + cat_len + 2u)); 

                        break;
                    }
                    case EC_EEPROM_CAT_RXPDO: {
                        do_eeprom_log(10, "EEPROM_RXPDO", "slave %2d:\n", slave);

                        // skip cat type and len
                        osal_uint32_t j = 0u;
                        osal_size_t local_offset = cat_offset + 2u;
                        if (!cat_len) {
                            break;
                        }

                        // freeing tailq first
                        ec_eeprom_cat_pdo_t *pdo = TAILQ_FIRST(&slv->eeprom.rxpdos);
                        while (pdo != NULL) {
                            TAILQ_REMOVE(&slv->eeprom.rxpdos, pdo, qh);
                            pdo = TAILQ_FIRST(&slv->eeprom.rxpdos);
                        }

                        do {
                            // read pdo
                            ec_eeprom_cat_pdo_t tmp_pdo;

                            (void)memset((osal_uint8_t *)&tmp_pdo, 0, sizeof(ec_eeprom_cat_pdo_t));
                            (void)ec_eepromread_len(pec, slave, local_offset, (osal_uint8_t *)&tmp_pdo, EC_EEPROM_CAT_PDO_LEN);
                            local_offset += (osal_size_t)(EC_EEPROM_CAT_PDO_LEN / 2u);

                            do_eeprom_log(10, "EEPROM_RXPDO", "          0x%04X, entries %d\n",
                                    tmp_pdo.pdo_index, tmp_pdo.n_entry);

                            if (tmp_pdo.n_entry > 0u) {
                                (void)memset(&tmp_pdo.entries[0], 0, sizeof(ec_eeprom_cat_pdo_entry_t) * LEC_MAX_EEPROM_CAT_PDO_ENTRIES);

                                for (j = 0; j < tmp_pdo.n_entry; ++j) {
                                    ec_eeprom_cat_pdo_entry_t entry;
                                    (void)ec_eepromread_len(pec, slave, local_offset,
                                            (osal_uint8_t *)&entry, 
                                            sizeof(ec_eeprom_cat_pdo_entry_t));

                                    local_offset += sizeof(ec_eeprom_cat_pdo_entry_t) / 2u;

                                    if (j >= LEC_MAX_EEPROM_CAT_PDO_ENTRIES) {
                                        do_eeprom_log(10, "EEPROM_RXPDO", 
                                                "          0x%04X:%2d -> 0x%04X (not stored)\n",
                                                tmp_pdo.pdo_index, j, entry.entry_index);
                                    } else {
                                        tmp_pdo.entries[j] = entry;

                                        do_eeprom_log(10, "EEPROM_RXPDO", 
                                                "          0x%04X:%2d -> 0x%04X\n",
                                                tmp_pdo.pdo_index, j, entry.entry_index);
                                    }
                                }
                            }

                            if (free_pdo_index < LEC_MAX_EEPROM_CAT_PDO_ENTRIES) {
                                pdo = &slv->eeprom.free_pdos[free_pdo_index];
                                free_pdo_index++;
                                memcpy(pdo, &tmp_pdo, sizeof(ec_eeprom_cat_pdo_t));
                                TAILQ_INSERT_TAIL(&slv->eeprom.rxpdos, pdo, qh);
                            }
                        } while (local_offset < (cat_offset + cat_len + 2u)); 

                        break;
                    }
                    case EC_EEPROM_CAT_DC: {
                        osal_uint32_t j = 0u;
                        osal_size_t local_offset = cat_offset + 2u;

                        do_eeprom_log(10, "EEPROM_DC", "slave %2d:\n", slave);

                        // allocating new dcs
                        osal_uint8_t dcs_cnt = cat_len / (osal_size_t)(EC_EEPROM_CAT_DC_LEN / 2u);
                        if (slv->eeprom.dcs_cnt > LEC_MAX_EEPROM_CAT_DC) {
                            ec_log(5, "EEPROM_DC", "slave %2d: can only store %" PRIu64 " dc settings but got %d!\n", 
                                    slave, LEC_MAX_EEPROM_CAT_DC, dcs_cnt);
                            slv->eeprom.dcs_cnt = LEC_MAX_EEPROM_CAT_DC;
                        } else {
                            slv->eeprom.dcs_cnt = dcs_cnt;
                        }

                        for (j = 0; j < dcs_cnt; ++j) {
                            ec_eeprom_cat_dc_t tmp_dc;
                            (void)ec_eepromread_len(pec, slave, local_offset, (osal_uint8_t *)&tmp_dc, EC_EEPROM_CAT_DC_LEN);
                            local_offset += (osal_size_t)(EC_EEPROM_CAT_DC_LEN / 2u);

                            do_eeprom_log(10, "EEPROM_DC", "          cycle_time_0 %d, "
                                    "shift_time_0 %d, shift_time_1 %d, "
                                    "sync_0_cycle_factor %d, sync_1_cycle_factor %d, "
                                    "assign_active %d\n", 
                                    tmp_dc.cycle_time_0, tmp_dc.shift_time_0, 
                                    tmp_dc.shift_time_1, tmp_dc.sync_0_cycle_factor, 
                                    tmp_dc.sync_1_cycle_factor, tmp_dc.assign_active);                   
                            if (j < LEC_MAX_EEPROM_CAT_DC) {
                                memcpy(&slv->eeprom.dcs[j], &tmp_dc, sizeof(ec_eeprom_cat_dc_t));
                            }
                        }

                        break;
                    }
                }

                cat_offset += cat_len + 2u; 

            } while (((cat_offset * 2u) < size) && (cat_type != EC_EEPROM_CAT_END));
        }
    
        slv->eeprom.read_eeprom = 1;
    }
}

