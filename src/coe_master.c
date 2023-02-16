//! ethercat canopen over ethercat for master mailbox handling
/*!
 * author: Robert Burger
 *
 * $Id$
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

#include <libethercat/config.h>

#include "libethercat/ec.h"
#include "libethercat/mbx.h"
#include "libethercat/coe.h"
#include "libethercat/error_codes.h"

// cppcheck-suppress misra-c2012-21.6
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

static ec_coe_sdo_desc_t obj_desc_master_0x1000 = { DEFTYPE_UNSIGNED32, OBJCODE_VAR, 0, "Device Type", 11 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x1000 =
    { 0, DEFTYPE_UNSIGNED32,    32, ACCESS_READ, "Device Type",       11 };
static ec_coe_sdo_desc_t obj_desc_master_0x1008 = { DEFTYPE_VISIBLESTRING, OBJCODE_VAR, 0, "Device Name", 11 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x1008 =
    { 0, DEFTYPE_VISIBLESTRING ,    8, ACCESS_READ, "Device Name",       11 };
static ec_coe_sdo_desc_t obj_desc_master_0x1009 = { DEFTYPE_VISIBLESTRING, OBJCODE_VAR, 0, "Manufacturer Hardware Version", 29 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x1009 =
    { 0, DEFTYPE_VISIBLESTRING ,    8, ACCESS_READ, "Manufacturer Hardware Version",       29 };
static ec_coe_sdo_desc_t obj_desc_master_0x100A = { DEFTYPE_VISIBLESTRING, OBJCODE_VAR, 0, "Manufacturer Software Version", 29 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x100A =
    { 0, DEFTYPE_VISIBLESTRING ,    8, ACCESS_READ, "Manufacturer Software Version",       29 };
static ec_coe_sdo_desc_t obj_desc_master_0x1018 = { DEFTYPE_RECORD, OBJCODE_REC, 4, "Identity", 8 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x1018[] = {
    { 0, DEFTYPE_UNSIGNED8 ,    8, ACCESS_READ, "Subindex 0",       10 }, // 0
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, "Vendor ID",         9 }, // 1
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, "Product Code",     12 }, // 1
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, "Revision Number",  15 }, // 1
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, "Serial Number",    13 } };

static ec_coe_sdo_desc_t obj_desc_master_0x8nnn = { DEFTYPE_RECORD, OBJCODE_REC, 35, "Configuration Data Slave", 24 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x8nnn[] = {
    { 0, DEFTYPE_UNSIGNED8 ,    8, ACCESS_READ, "Subindex 0"           , 10 }, // 0
    { 0, DEFTYPE_UNSIGNED16,   16, ACCESS_READ, "Fixed Station Address", 21 }, // 1
    { 0, DEFTYPE_VISIBLESTRING, 8, ACCESS_READ, "Type",                   4 }, // 2
    { 0, DEFTYPE_VISIBLESTRING, 8, ACCESS_READ, "Name",                   4 }, // 3
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, "Device Type",           11 }, // 4
    { 0, DEFTYPE_DWORD,        32, ACCESS_READ, "Vendor Id",              9 }, // 5
    { 0, DEFTYPE_DWORD,        32, ACCESS_READ, "Product Code",          12 }, // 6
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, "Revision Number",       15 }, // 7
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, "Serial Number",         13 }, // 8 
    { 0,                  0,    0,           0, "",                       0 }, // 9
    { 0,                  0,    0,           0, "",                       0 }, // 10
    { 0,                  0,    0,           0, "",                       0 }, // 11
    { 0,                  0,    0,           0, "",                       0 }, // 12
    { 0,                  0,    0,           0, "",                       0 }, // 13
    { 0,                  0,    0,           0, "",                       0 }, // 14
    { 0,                  0,    0,           0, "",                       0 }, // 15
    { 0,                  0,    0,           0, "",                       0 }, // 16
    { 0,                  0,    0,           0, "",                       0 }, // 17
    { 0,                  0,    0,           0, "",                       0 }, // 18
    { 0,                  0,    0,           0, "",                       0 }, // 19
    { 0,                  0,    0,           0, "",                       0 }, // 20
    { 0,                  0,    0,           0, "",                       0 }, // 21
    { 0,                  0,    0,           0, "",                       0 }, // 22
    { 0,                  0,    0,           0, "",                       0 }, // 23
    { 0,                  0,    0,           0, "",                       0 }, // 24
    { 0,                  0,    0,           0, "",                       0 }, // 25
    { 0,                  0,    0,           0, "",                       0 }, // 26
    { 0,                  0,    0,           0, "",                       0 }, // 27
    { 0,                  0,    0,           0, "",                       0 }, // 28
    { 0,                  0,    0,           0, "",                       0 }, // 29
    { 0,                  0,    0,           0, "",                       0 }, // 30
    { 0,                  0,    0,           0, "",                       0 }, // 31
    { 0,                  0,    0,           0, "",                       0 }, // 32
    { 0, DEFTYPE_UNSIGNED16,   16, ACCESS_READ, "Mailbox Out Size",      16 }, // 33
    { 0, DEFTYPE_UNSIGNED16,   16, ACCESS_READ, "Mailbox In Size",       15 }, // 34
    { 0, DEFTYPE_BYTE,          8, ACCESS_READ, "Link Status",           11 }, // 35
};

static ec_coe_sdo_desc_t obj_desc_master_0x9nnn = { DEFTYPE_RECORD, OBJCODE_REC, 32, "Information Data Slave", 22 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0x9nnn[] = {
    { 0, DEFTYPE_UNSIGNED8 ,    8, ACCESS_READ, "Subindex 0"           , 10 }, // 0
    { 0, DEFTYPE_UNSIGNED16,   16, ACCESS_READ, "Fixed Station Address", 21 }, // 1
    { 0,                  0,    0,           0, "",                       0 }, // 2
    { 0,                  0,    0,           0, "",                       0 }, // 3
    { 0,                  0,    0,           0, "",                       0 }, // 4
    { 0, DEFTYPE_DWORD,        32, ACCESS_READ, "Vendor Id",              9 }, // 5
    { 0, DEFTYPE_DWORD,        32, ACCESS_READ, "Product Code",          12 }, // 6
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, "Revision Number",       15 }, // 7
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, "Serial Number",         13 }, // 8 
    { 0,                  0,    0,           0, "",                       0 }, // 9
    { 0,                  0,    0,           0, "",                       0 }, // 10
    { 0,                  0,    0,           0, "",                       0 }, // 11
    { 0,                  0,    0,           0, "",                       0 }, // 12
    { 0,                  0,    0,           0, "",                       0 }, // 13
    { 0,                  0,    0,           0, "",                       0 }, // 14
    { 0,                  0,    0,           0, "",                       0 }, // 15
    { 0,                  0,    0,           0, "",                       0 }, // 16
    { 0,                  0,    0,           0, "",                       0 }, // 17
    { 0,                  0,    0,           0, "",                       0 }, // 18
    { 0,                  0,    0,           0, "",                       0 }, // 19
    { 0,                  0,    0,           0, "",                       0 }, // 20
    { 0,                  0,    0,           0, "",                       0 }, // 21
    { 0,                  0,    0,           0, "",                       0 }, // 22
    { 0,                  0,    0,           0, "",                       0 }, // 23
    { 0,                  0,    0,           0, "",                       0 }, // 24
    { 0,                  0,    0,           0, "",                       0 }, // 25
    { 0,                  0,    0,           0, "",                       0 }, // 26
    { 0,                  0,    0,           0, "",                       0 }, // 27
    { 0,                  0,    0,           0, "",                       0 }, // 28
    { 0,                  0,    0,           0, "",                       0 }, // 29
    { 0,                  0,    0,           0, "",                       0 }, // 30
    { 0,                  0,    0,           0, "",                       0 }, // 31
    { 0, DEFTYPE_WORD,         16, ACCESS_READ, "DL Status Register",    18 }, // 32
};

static ec_coe_sdo_desc_t obj_desc_master_0xAnnn = { DEFTYPE_RECORD, OBJCODE_REC, 2, "Diagnosis Data Slave", 20 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0xAnnn[] = {
    { 0, DEFTYPE_UNSIGNED8,    8, ACCESS_READ,      "Subindex 0",            10 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      "AL Status",              9 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      "AL Control",            10 },
};

static ec_coe_sdo_desc_t obj_desc_master_0xF000 = { DEFTYPE_RECORD, OBJCODE_REC, 4, "Modular Device Profile", 22 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0xF000[] = {
    { 0, DEFTYPE_UNSIGNED8,    8, ACCESS_READ,      "Subindex 0",            10 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      "Module Index Distance", 21 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      "Maximum Number of Modules", 25 },
    { 0, DEFTYPE_UNSIGNED32,  32, ACCESS_READ,      "General Configuration", 21 },
    { 0, DEFTYPE_UNSIGNED32,  32, ACCESS_READ,      "General Information",   19 },
};
static osal_uint8_t  element_master_0xF000_00 = 4;
static osal_uint16_t element_master_0xF000_01 = 0x0001;
static osal_uint16_t element_master_0xF000_02 = 4080;
static osal_uint32_t element_master_0xF000_03 = 0x000000FF;
static osal_uint32_t element_master_0xF000_04 = 0x000000F1;

static ec_coe_sdo_desc_t obj_desc_master_0xF002 = { DEFTYPE_RECORD, OBJCODE_REC, 3, "Detect Modules Command", 22 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0xF002[] = {
    { 0, DEFTYPE_UNSIGNED8,    8, ACCESS_READ,      "Subindex 0",            10 },
    { 0, DEFTYPE_OCTETSTRING, 16, ACCESS_READWRITE, "Scan Command Request",  20 },
    { 0, DEFTYPE_UNSIGNED8,    8, ACCESS_READ,      "Scan Command Status",   19 },
    { 0, DEFTYPE_OCTETSTRING, 48, ACCESS_READ,      "Scan Command Response", 21 }
};

static ec_coe_sdo_desc_t obj_desc_master_0xF02n = { DEFTYPE_ARRAY_OF_INT, OBJCODE_ARR, 255, "Configured Address List Slaves", 30 }; 
static ec_coe_sdo_entry_desc_t entry_desc_master_0xF02n[] = {
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      "Subindex Slave",        14 },
};

static ec_coe_sdo_desc_t obj_desc_master_0xF04n = { DEFTYPE_ARRAY_OF_INT, OBJCODE_ARR, 255, "Detected Address List Slaves", 28 }; 
//static ec_coe_sdo_entry_desc_t entry_desc_master_0xF04n[] = {
//    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      "Subindex Slave",        14 },
//};

// Read CoE service data object (SDO) of master
int ec_coe_master_sdo_read(ec_t *pec, osal_uint16_t index, 
        osal_uint8_t sub_index, int complete, osal_uint8_t *buf, osal_size_t *len, 
        osal_uint32_t *abort_code) {
    assert(pec != NULL);
    assert(buf != NULL);
    assert(len != NULL);

    int ret = EC_OK;

    if (index == 0x1000) {
        if (sub_index == 0) {
            if ((*len) >= 4) {
                (*(osal_uint32_t *)buf) = 0;
                (*len) = sizeof(osal_uint32_t);
            }
        } 
    } else if (index == 0x1008) {
        if (sub_index == 0) {
            if ((*len) >= strlen(LIBETHERCAT_PACKAGE_NAME)) {
                (void)strncpy((char *)buf, LIBETHERCAT_PACKAGE_NAME, *(len));
            }          
                
            (*len) = strlen(LIBETHERCAT_PACKAGE_NAME);
        }
    } else if (index == 0x1009) {
        if (sub_index == 0) {
            if ((*len) >= strlen("0.0.0")) {
                (void)strncpy((char *)buf, "0.0.0", *(len));
            }          
                
            (*len) = strlen("0.0.0");
        }
    } else if (index == 0x100A) {
        if (sub_index == 0) {
            if ((*len) >= strlen(LIBETHERCAT_PACKAGE_VERSION)) {
                (void)strncpy((char *)buf, LIBETHERCAT_PACKAGE_VERSION, *(len));
            }          
                
            (*len) = strlen(LIBETHERCAT_PACKAGE_VERSION);
        }
    } else if (index == 0x1018) {
        if (sub_index == 0) {
            if ((*len) >= 1) {
                (*buf) = 4;
                (*len) = sizeof(osal_uint8_t);
            }
        } else if (sub_index == 1) {
            if ((*len) >= 4) {
                (*(osal_uint32_t *)buf) = 0x1616;
                (*len) = sizeof(osal_uint32_t);
            }
        } else if (sub_index == 2) {
            if ((*len) >= 4) {
                (*(osal_uint32_t *)buf) = 0x11BECA7;
                (*len) = sizeof(osal_uint32_t);
            }
        } else if (sub_index == 3) {
            if ((*len) >= 4) {
                (*(osal_uint32_t *)buf) = 0x0;
                (*len) = sizeof(osal_uint32_t);
            }
        } else if (sub_index == 4) {
            if ((*len) >= 4) {
                (*(osal_uint32_t *)buf) = 0x0;
                (*len) = sizeof(osal_uint32_t);
            }
        }
    } else if ((index & 0xF000) == 0x8000) {
        osal_uint16_t slave = index & 0x0FFF;
        if (sub_index == 0) {
            if ((*len) >= 1) {
                (*buf) = 35;
                (*len) = sizeof(element_master_0xF000_00);
            }
        } else if (sub_index == 1) {
            if ((*len) >= 2) {
                (*(osal_uint16_t *)buf) = pec->slaves[slave].fixed_address;
                (*len) = sizeof(osal_uint16_t);
            }
        } else if (sub_index == 2) {
            if (pec->slaves[slave].eeprom.mbx_supported != 0) {
                ret = ec_coe_sdo_read(pec, slave, 0x100A, 0, complete, buf, len, abort_code);
            }
        } else if (sub_index == 3) {
            if (pec->slaves[slave].eeprom.mbx_supported != 0) {
                ret = ec_coe_sdo_read(pec, slave, 0x1008, 0, complete, buf, len, abort_code);
            }
        } else if (sub_index == 4) {
            if (pec->slaves[slave].eeprom.mbx_supported != 0) {
                ret = ec_coe_sdo_read(pec, slave, 0x1000, 0, complete, buf, len, abort_code);
            }
        } else if ((sub_index == 5) || (sub_index == 6) || (sub_index == 7) || (sub_index == 8)) {
            ret = ec_coe_sdo_read(pec, slave, 0x1018, sub_index - 4, complete, buf, len, abort_code);
        } else if ((sub_index == 33) || (sub_index == 34)) {
            if ((*len) >= 2) {
                if (pec->slaves[slave].eeprom.mbx_supported != 0) {
                    (*(osal_uint16_t *)buf) = pec->slaves[slave].sm[sub_index-33].len;
                    (*len) = sizeof(osal_uint16_t);
                } else {
                    (*(osal_uint16_t *)buf) = 0;
                    (*len) = sizeof(osal_uint16_t);
                }
            }
        } else if (sub_index == 35) {
            if ((*len) >= 1) {
                ec_reg_dl_status_t dl_stat;
                osal_uint16_t wkc = 0;
                (void)ec_fprd(pec, pec->slaves[slave].fixed_address, 0x110, (osal_uint16_t *)&dl_stat, sizeof(osal_uint16_t), &wkc);

                if (wkc != 0) {
                    (*(osal_uint8_t *)buf) = 
                        (dl_stat.link_status_port_0 << 0u) |
                        (dl_stat.loop_status_port_0 << 1u) |
                        (dl_stat.link_status_port_1 << 2u) |
                        (dl_stat.loop_status_port_1 << 3u) |
                        (dl_stat.link_status_port_2 << 4u) |
                        (dl_stat.loop_status_port_2 << 5u) |
                        (dl_stat.link_status_port_3 << 6u) |
                        (dl_stat.loop_status_port_3 << 7u);
                    (*len) = sizeof(osal_uint8_t);
                }
            }
        } else {
            ret = EC_ERROR_MAILBOX_ABORT;
        }
    } else if ((index & 0xF000) == 0x9000) {
        osal_uint16_t slave = index & 0x0FFF;
        if (sub_index == 0) {
            if ((*len) >= 1) {
                (*buf) = 32;
                (*len) = sizeof(element_master_0xF000_00);
            }
        } else if (sub_index == 1) {
            if ((*len) >= 2) {
                (*(osal_uint16_t *)buf) = pec->slaves[slave].fixed_address;
                (*len) = sizeof(osal_uint16_t);
            }
        } else if ((sub_index == 5) || (sub_index == 6) || (sub_index == 7) || (sub_index == 8)) {
            ret = ec_coe_sdo_read(pec, slave, 0x1018, sub_index - 4, complete, buf, len, abort_code);
        } else if (sub_index == 32) {
            if ((*len) >= 2) {
                osal_uint16_t wkc = 0;
                (void)ec_fprd(pec, pec->slaves[slave].fixed_address, 0x110, buf, sizeof(osal_uint16_t), &wkc);

                if (wkc != 0) {
                    (*len) = sizeof(osal_uint16_t);
                }
            }
        } else {
            ret = EC_ERROR_MAILBOX_ABORT;
        }
    } else if ((index & 0xF000) == 0xA000) {
        osal_uint16_t slave = index & 0x0FFF;
        if (sub_index == 0) {
            if ((*len) >= 1) {
                (*buf) = 2;
                (*len) = sizeof(element_master_0xF000_00);
            }
        } else if (sub_index == 1) {
            if ((*len) >= 2) {
                osal_uint16_t wkc = 0;
                (void)ec_fprd(pec, pec->slaves[slave].fixed_address, 0x130, buf, sizeof(osal_uint16_t), &wkc);

                if (wkc != 0) {
                    (*len) = sizeof(osal_uint16_t);
                }
            }
        } else if (sub_index == 2) {
            if ((*len) >= 2) {
                osal_uint16_t wkc = 0;
                (void)ec_fprd(pec, pec->slaves[slave].fixed_address, 0x120, buf, sizeof(osal_uint16_t), &wkc);

                if (wkc != 0) {
                    (*len) = sizeof(osal_uint16_t);
                }
            }
        } else {
            ret = EC_ERROR_MAILBOX_ABORT;
        }

    } else if (index == 0xF000) {
        if (sub_index == 0) {
            if ((*len) >= 1) {
                (void)memcpy(buf, &element_master_0xF000_00, sizeof(element_master_0xF000_00));
                (*len) = sizeof(element_master_0xF000_00);
            }
        } else if (sub_index == 1) {
            if ((*len) >= 2) {
                (void)memcpy(buf, &element_master_0xF000_01, sizeof(element_master_0xF000_01));
                (*len) = sizeof(element_master_0xF000_01);
            }
        } else if (sub_index == 2) {
            if ((*len) >= 2) {
                (void)memcpy(buf, &element_master_0xF000_02, sizeof(element_master_0xF000_02));
                (*len) = sizeof(element_master_0xF000_02);
            }
        } else if (sub_index == 3) {
            if ((*len) >= 4) {
                (void)memcpy(buf, &element_master_0xF000_03, sizeof(element_master_0xF000_03));
                (*len) = sizeof(element_master_0xF000_03);
            }
        } else if (sub_index == 4) {
            if ((*len) >= 4) {
                (void)memcpy(buf, &element_master_0xF000_04, sizeof(element_master_0xF000_04));
                (*len) = sizeof(element_master_0xF000_04);
            }
        } 
    } else if (((index & 0xFFF0) == 0xF020) || ((index & 0xFFF0) == 0xF040)) {
        osal_uint16_t slave_range = index & 0x000Fu;
        osal_uint16_t slave = (slave_range * 255) + (sub_index - 1);

        if ((*len) >= 2) {
            if (slave < pec->slave_cnt) {
                (*(osal_uint16_t *)buf) = pec->slaves[slave].fixed_address;
            } else {
                (*(osal_uint16_t *)buf) = 0;
            }
            
            (*len) = sizeof(osal_uint16_t);
        }
    } else {
        ret = EC_ERROR_MAILBOX_ABORT;
    }

    return ret;
}

// read coe object dictionary list of master
int ec_coe_master_odlist_read(ec_t *pec, osal_uint8_t *buf, osal_size_t *len) {
    assert(pec != NULL);
    assert(buf != NULL);
    assert(len != NULL);
    int ret = EC_OK;

    // return just an uint16_t array of indices
    //uint16_t test_indices[] = { 0x8000, 0x9000, 0xA000, 0xF000, 0xF002, 0xF020, 0xF040 };
    //osal_size_t od_len = sizeof(uint16_t) * 7;//sizeof(test_indices);

    osal_uint8_t *tmp = buf;
    osal_uint8_t *end = buf + *len;

    if (tmp < end) {
        *(osal_uint16_t *)tmp = 0x1000;
        tmp += sizeof(osal_uint16_t);
    }
    
    if (tmp < end) {
        *(osal_uint16_t *)tmp = 0x1008;
        tmp += sizeof(osal_uint16_t);
    }
    
    if (tmp < end) {
        *(osal_uint16_t *)tmp = 0x1009;
        tmp += sizeof(osal_uint16_t);
    }

    if (tmp < end) {
        *(osal_uint16_t *)tmp = 0x100A;
        tmp += sizeof(osal_uint16_t);
    }
    
    if (tmp < end) {
        *(osal_uint16_t *)tmp = 0x1018;
        tmp += sizeof(osal_uint16_t);
    }

    for (int i = 0; (i < pec->slave_cnt) && (tmp < end); ++i) {
        *(osal_uint16_t *)tmp = 0x8000 | i;
        tmp += sizeof(osal_uint16_t);
    }
    
    for (int i = 0; (i < pec->slave_cnt) && (tmp < end); ++i) {
        *(osal_uint16_t *)tmp = 0x9000 | i;
        tmp += sizeof(osal_uint16_t);
    }

    for (int i = 0; (i < pec->slave_cnt) && (tmp < end); ++i) {
        *(osal_uint16_t *)tmp = 0xA000 | i;
        tmp += sizeof(osal_uint16_t);
    }

    if (tmp < end) {
        *(osal_uint16_t *)tmp = 0xF000;
        tmp += sizeof(osal_uint16_t);
    }

    if (tmp < end) {
        *(osal_uint16_t *)tmp = 0xF002;
        tmp += sizeof(osal_uint16_t);
    }
    
    for (int i = 0; (i < ((pec->slave_cnt/255) + 1)) && (tmp < end); ++i) {
        *(osal_uint16_t *)tmp = 0xF020 | i;
        tmp += sizeof(osal_uint16_t);
    }

    for (int i = 0; (i < ((pec->slave_cnt/255) + 1)) && (tmp < end); ++i) {
        *(osal_uint16_t *)tmp = 0xF040 | i;
        tmp += sizeof(osal_uint16_t);
    }

    if (tmp >= end) {
        ret = EC_ERROR_MAILBOX_BUFFER_TOO_SMALL;
    } else {
        (*len) = tmp - buf;
    }

    return ret;
}

// Read CoE SDO description of master
int ec_coe_master_sdo_desc_read(const ec_t *pec, osal_uint16_t index, 
        ec_coe_sdo_desc_t *desc, osal_uint32_t *error_code) 
{
    assert(pec != NULL);
    assert(desc != NULL);
    (void)error_code;

    int ret = EC_OK;

    if (index == 0x1000) {
        (void)memcpy(desc, &obj_desc_master_0x1000, sizeof(obj_desc_master_0x1000));
        desc->name_len = strlen(desc->name);
    } else if (index == 0x1008) {
        (void)memcpy(desc, &obj_desc_master_0x1008, sizeof(obj_desc_master_0x1008));
        desc->name_len = strlen(desc->name);
    } else if (index == 0x1009) {
        (void)memcpy(desc, &obj_desc_master_0x1009, sizeof(obj_desc_master_0x1009));
        desc->name_len = strlen(desc->name);
    } else if (index == 0x100A) {
        (void)memcpy(desc, &obj_desc_master_0x100A, sizeof(obj_desc_master_0x100A));
        desc->name_len = strlen(desc->name);
    } else if (index == 0x1018) {
        (void)memcpy(desc, &obj_desc_master_0x1018, sizeof(obj_desc_master_0x1018));
        desc->name_len = strlen(desc->name);
    } else if ((index & 0xF000) == 0x8000) {
        osal_uint16_t slave = index & 0x0FFF;
        (void)memcpy(desc, &obj_desc_master_0x8nnn, sizeof(obj_desc_master_0x8nnn));
        (void)snprintf(&desc->name[0], CANOPEN_MAXNAME, "Configuration Data Slave %hu", slave);
        desc->name_len = strlen(desc->name);
    } else if ((index & 0xF000) == 0x9000) {
        osal_uint16_t slave = index & 0x0FFF;
        (void)memcpy(desc, &obj_desc_master_0x9nnn, sizeof(obj_desc_master_0x9nnn));
        (void)snprintf(&desc->name[0], CANOPEN_MAXNAME, "Information Data Slave %hu", slave);
        desc->name_len = strlen(desc->name);
    } else if ((index & 0xF000) == 0xA000) {
        osal_uint16_t slave = index & 0x0FFF;
        (void)memcpy(desc, &obj_desc_master_0xAnnn, sizeof(obj_desc_master_0xAnnn));
        (void)snprintf(&desc->name[0], CANOPEN_MAXNAME, "Diagnosis Data Slave %hu", slave);
        desc->name_len = strlen(desc->name);
    } else if ((index & 0xF000) == 0xF000) {
        if (index == 0xF000) {
            (void)memcpy(desc, &obj_desc_master_0xF000, sizeof(obj_desc_master_0xF000));
            desc->name_len = strlen(desc->name);
        } else if (index == 0xF002) {
            (void)memcpy(desc, &obj_desc_master_0xF002, sizeof(obj_desc_master_0xF002));
            desc->name_len = strlen(desc->name);
        } else {
            osal_uint16_t slave_range = index & 0x000F;
            osal_uint16_t slave_begin = slave_range * 256;
            osal_uint16_t slave_end = slave_begin + 255;
        
            if ((index & 0xF020) == 0xF020) {
                (void)memcpy(desc, &obj_desc_master_0xF02n, sizeof(obj_desc_master_0xF02n));
                (void)snprintf(&desc->name[0], CANOPEN_MAXNAME, "Configured Address List %hu-%hu", slave_begin, slave_end);
                desc->name_len = strlen(desc->name);
            } else if ((index & 0xF040) == 0xF040) {
                (void)memcpy(desc, &obj_desc_master_0xF04n, sizeof(obj_desc_master_0xF04n));
                (void)snprintf(&desc->name[0], CANOPEN_MAXNAME, "Detected Address List %hu-%hu", slave_begin, slave_end);
                desc->name_len = strlen(desc->name);
            }
        }
    }

    return ret;
}

// Read CoE SDO entry description of master
int ec_coe_master_sdo_entry_desc_read(const ec_t *pec, osal_uint16_t index,
        osal_uint8_t sub_index, osal_uint8_t value_info, ec_coe_sdo_entry_desc_t *desc, 
        osal_uint32_t *error_code) 
{
    assert(pec != NULL);
    assert(desc != NULL);
    (void)value_info;
    (void)error_code;

    int ret = EC_OK; 

#define if_index(x) \
    if (index == (x)) { \
        memcpy(desc, &entry_desc_master_##x, sizeof(ec_coe_sdo_entry_desc_t)); \
    }

    if_index(0x1000)
    else if_index(0x1008)
    else if_index(0x1009)
    else if_index(0x100A)
    else if (index == 0x1018) {
        if (sub_index <= 4) {
            (void)memcpy(desc, &entry_desc_master_0x1018[sub_index], sizeof(ec_coe_sdo_entry_desc_t));
        }
    } else if ((index & 0xF000) == 0x8000) {
        if (sub_index <= 35) {
            (void)memcpy(desc, &entry_desc_master_0x8nnn[sub_index], sizeof(ec_coe_sdo_entry_desc_t));
        }
    } else if ((index & 0xF000) == 0x9000) {
        if (sub_index <= 32) {
            (void)memcpy(desc, &entry_desc_master_0x9nnn[sub_index], sizeof(ec_coe_sdo_entry_desc_t));
        }
    } else if ((index & 0xF000) == 0xA000) {
        if (sub_index <= 2) {
            (void)memcpy(desc, &entry_desc_master_0xAnnn[sub_index], sizeof(ec_coe_sdo_entry_desc_t));
        }
    } else if (index == 0xF000) {
        if (sub_index < 5) {
            (void)memcpy(desc, &entry_desc_master_0xF000[sub_index], sizeof(ec_coe_sdo_entry_desc_t));
        }
    } else if (index == 0xF002) {
        if (sub_index < 4) {
            (void)memcpy(desc, &entry_desc_master_0xF002[sub_index], sizeof(ec_coe_sdo_entry_desc_t));
        }
    } else if (((index & 0xFFF0) == 0xF020) || ((index & 0xFFF0) == 0xF040)) {
        osal_uint16_t slave_range = index & 0x000Fu;
        osal_uint16_t slave = (slave_range * 255) + (sub_index - 1);

        (void)memcpy(desc, &entry_desc_master_0xF02n[0], sizeof(ec_coe_sdo_entry_desc_t));
        (void)snprintf((char *)&desc->data[0], CANOPEN_MAXNAME, "Subindex Slave %hu", slave);
        desc->data_len = strlen((char *)&desc->data[0]);
    }

    return ret;
}

