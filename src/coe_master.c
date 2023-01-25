//! ethercat canopen over ethercat mailbox handling
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

ec_coe_sdo_desc_t obj_desc_master_0x8nnn = { DEFTYPE_RECORD, OBJCODE_REC, 35, "Configuration Data Slave", 24 }; 
ec_coe_sdo_entry_desc_t entry_desc_master_0x8nnn[] = {
    { 0, DEFTYPE_UNSIGNED8 ,    8, ACCESS_READ, "Subindex 0"           , 10 }, // 0
    { 0, DEFTYPE_UNSIGNED16,    8, ACCESS_READ, "Fixed Station Address", 21 }, // 1
    { 0, DEFTYPE_VISIBLESTRING, 8, ACCESS_READ, "Type",                   4 }, // 2
    { 0, DEFTYPE_VISIBLESTRING, 8, ACCESS_READ, "Name",                   4 }, // 3
    { 0, DEFTYPE_UNSIGNED32,   32, ACCESS_READ, "Device Type",           11 }, // 4
    { 0, DEFTYPE_UNSIGNED32,    8, ACCESS_READ, "Vendor Id",              9 }, // 5
    { 0, DEFTYPE_UNSIGNED32,    8, ACCESS_READ, "Product Code",          12 }, // 6
    { 0, DEFTYPE_UNSIGNED32,    8, ACCESS_READ, "Revision Number",       15 }, // 7
    { 0, DEFTYPE_UNSIGNED32,    8, ACCESS_READ, "Serial Number",         13 }, // 8 
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

ec_coe_sdo_desc_t obj_desc_master_0x9nnn = { DEFTYPE_RECORD, OBJCODE_REC, 32, "Information Data Slave", 22 }; 
ec_coe_sdo_entry_desc_t entry_desc_master_0x9nnn[] = {
    { 0, DEFTYPE_UNSIGNED8 ,    8, ACCESS_READ, "Subindex 0"           , 10 }, // 0
    { 0, DEFTYPE_UNSIGNED16,    8, ACCESS_READ, "Fixed Station Address", 21 }, // 1
    { 0,                  0,    0,           0, "",                       0 }, // 2
    { 0,                  0,    0,           0, "",                       0 }, // 3
    { 0,                  0,    0,           0, "",                       0 }, // 4
    { 0, DEFTYPE_UNSIGNED32,    8, ACCESS_READ, "Vendor Id",              9 }, // 5
    { 0, DEFTYPE_UNSIGNED32,    8, ACCESS_READ, "Product Code",          12 }, // 6
    { 0, DEFTYPE_UNSIGNED32,    8, ACCESS_READ, "Revision Number",       15 }, // 7
    { 0, DEFTYPE_UNSIGNED32,    8, ACCESS_READ, "Serial Number",         13 }, // 8 
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
    { 0, DEFTYPE_UNSIGNED16,   16, ACCESS_READ, "DL Status Register",    18 }, // 32
};

ec_coe_sdo_desc_t obj_desc_master_0xAnnn = { DEFTYPE_RECORD, OBJCODE_REC, 2, "Diagnosis Data Slave", 20 }; 
ec_coe_sdo_entry_desc_t entry_desc_master_0xAnnn[] = {
    { 0, DEFTYPE_UNSIGNED8,    8, ACCESS_READ,      "Subindex 0",            10 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      "AL Status",              9 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      "AL Control",            10 },
};

ec_coe_sdo_desc_t obj_desc_master_0xF000 = { DEFTYPE_RECORD, OBJCODE_REC, 4, "Modular Device Profile", 22 }; 
ec_coe_sdo_entry_desc_t entry_desc_master_0xF000[] = {
    { 0, DEFTYPE_UNSIGNED8,    8, ACCESS_READ,      "Subindex 0",            10 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      "Module Index Distance", 21 },
    { 0, DEFTYPE_UNSIGNED16,  16, ACCESS_READ,      "Maximum Number of Modules", 25 },
    { 0, DEFTYPE_UNSIGNED32,  32, ACCESS_READ,      "General Configuration", 21 },
    { 0, DEFTYPE_UNSIGNED32,  32, ACCESS_READ,      "General Information",   19 },
};
osal_uint8_t  element_master_0xF000_00 = 4;
osal_uint16_t element_master_0xF000_01 = 0x0001;
osal_uint16_t element_master_0xF000_02 = 4080;
osal_uint32_t element_master_0xF000_03 = 0x000000FF;
osal_uint32_t element_master_0xF000_04 = 0x000000F1;

ec_coe_sdo_desc_t obj_desc_master_0xF002 = { DEFTYPE_RECORD, OBJCODE_REC, 3, "Detect Modules Command", 22 }; 
ec_coe_sdo_entry_desc_t entry_desc_master_0xF002[] = {
    { 0, DEFTYPE_UNSIGNED8,    8, ACCESS_READ,      "Subindex 0",            10 },
    { 0, DEFTYPE_OCTETSTRING, 16, ACCESS_READWRITE, "Scan Command Request",  20 },
    { 0, DEFTYPE_UNSIGNED8,    8, ACCESS_READ,      "Scan Command Status",   19 },
    { 0, DEFTYPE_OCTETSTRING, 48, ACCESS_READ,      "Scan Command Response", 21 }
};

ec_coe_sdo_desc_t obj_desc_master_0xF02n = { DEFTYPE_RECORD, OBJCODE_REC, 255, "Configured Address List Slaves", 30 }; 
ec_coe_sdo_desc_t obj_desc_master_0xF04n = { DEFTYPE_RECORD, OBJCODE_REC, 255, "Detected Address List Slaves", 28 }; 

// Read CoE service data object (SDO) of master
int ec_coe_master_sdo_read(ec_t *pec, osal_uint16_t index, 
        osal_uint8_t sub_index, int complete, osal_uint8_t *buf, osal_size_t *len, 
        osal_uint32_t *abort_code) {
    assert(pec != NULL);
    assert(buf != NULL);
    assert(len != NULL);

    int ret = EC_OK;

    if ((index & 0xF000) == 0x8000) {
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
                ec_fprd(pec, pec->slaves[slave].fixed_address, 0x110, buf, sizeof(osal_uint16_t), &wkc);

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
                ec_fprd(pec, pec->slaves[slave].fixed_address, 0x130, buf, sizeof(osal_uint16_t), &wkc);

                if (wkc != 0) {
                    (*len) = sizeof(osal_uint16_t);
                }
            }
        } else if (sub_index == 2) {
            if ((*len) >= 2) {
                osal_uint16_t wkc = 0;
                ec_fprd(pec, pec->slaves[slave].fixed_address, 0x120, buf, sizeof(osal_uint16_t), &wkc);

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
                memcpy(buf, &element_master_0xF000_00, sizeof(element_master_0xF000_00));
                (*len) = sizeof(element_master_0xF000_00);
            }
        } else if (sub_index == 1) {
            if ((*len) >= 2) {
                memcpy(buf, &element_master_0xF000_01, sizeof(element_master_0xF000_01));
                (*len) = sizeof(element_master_0xF000_01);
            }
        } else if (sub_index == 2) {
            if ((*len) >= 2) {
                memcpy(buf, &element_master_0xF000_02, sizeof(element_master_0xF000_02));
                (*len) = sizeof(element_master_0xF000_02);
            }
        } else if (sub_index == 3) {
            if ((*len) >= 4) {
                memcpy(buf, &element_master_0xF000_03, sizeof(element_master_0xF000_03));
                (*len) = sizeof(element_master_0xF000_03);
            }
        } else if (sub_index == 4) {
            if ((*len) >= 4) {
                memcpy(buf, &element_master_0xF000_04, sizeof(element_master_0xF000_04));
                (*len) = sizeof(element_master_0xF000_04);
            }
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
    uint16_t test_indices[] = { 0x8000, 0x9000, 0xA000, 0xF000, 0xF002, 0xF020, 0xF040 };
    osal_size_t od_len = sizeof(uint16_t) * 7;//sizeof(test_indices);

    if ((*len) < od_len) {
        ret = EC_ERROR_MAILBOX_BUFFER_TOO_SMALL;
    }

    (*len) = od_len;

    if (ret == EC_OK) {
        memcpy(buf, &test_indices[0], od_len);
    }

    return ret;
}

// Read CoE SDO description of master
int ec_coe_master_sdo_desc_read(ec_t *pec, osal_uint16_t index, 
        ec_coe_sdo_desc_t *desc, osal_uint32_t *error_code) {
    assert(pec != NULL);
    assert(desc != NULL);

    int ret = EC_OK;

    if ((index & 0xF000) == 0x8000) {
        osal_uint16_t slave = index & 0x0FFF;
        memcpy(desc, &obj_desc_master_0x8nnn, sizeof(obj_desc_master_0x8nnn));
        snprintf(&desc->name[0], CANOPEN_MAXNAME, "Configuration Data Slave %hu", slave);
        desc->name_len = strlen(desc->name);
    } else if ((index & 0xF000) == 0x9000) {
        osal_uint16_t slave = index & 0x0FFF;
        memcpy(desc, &obj_desc_master_0x9nnn, sizeof(obj_desc_master_0x9nnn));
        snprintf(&desc->name[0], CANOPEN_MAXNAME, "Information Data Slave %hu", slave);
        desc->name_len = strlen(desc->name);
    } else if ((index & 0xF000) == 0xA000) {
        osal_uint16_t slave = index & 0x0FFF;
        memcpy(desc, &obj_desc_master_0xAnnn, sizeof(obj_desc_master_0xAnnn));
        snprintf(&desc->name[0], CANOPEN_MAXNAME, "Diagnosis Data Slave %hu", slave);
        desc->name_len = strlen(desc->name);
    } else if ((index & 0xF000) == 0xF000) {
        if (index == 0xF000) {
            memcpy(desc, &obj_desc_master_0xF000, sizeof(obj_desc_master_0xF000));
            desc->name_len = strlen(desc->name);
        } else if (index == 0xF002) {
            memcpy(desc, &obj_desc_master_0xF002, sizeof(obj_desc_master_0xF002));
            desc->name_len = strlen(desc->name);
        } else {
            osal_uint16_t slave_range = index & 0x000F;
            osal_uint16_t slave_begin = slave_range * 256;
            osal_uint16_t slave_end = slave_begin + 255;
        
            if ((index & 0xF020) == 0xF020) {
                memcpy(desc, &obj_desc_master_0xF02n, sizeof(obj_desc_master_0xF02n));
                snprintf(&desc->name[0], CANOPEN_MAXNAME, "Configured Address List %hu-%hu", slave_begin, slave_end);
                desc->name_len = strlen(desc->name);
            } else if ((index & 0xF040) == 0xF040) {
                memcpy(desc, &obj_desc_master_0xF04n, sizeof(obj_desc_master_0xF04n));
                snprintf(&desc->name[0], CANOPEN_MAXNAME, "Detected Address List %hu-%hu", slave_begin, slave_end);
                desc->name_len = strlen(desc->name);
            }
        }
    }

    return ret;
}

// Read CoE SDO entry description of master
int ec_coe_master_sdo_entry_desc_read(ec_t *pec, osal_uint16_t index,
        osal_uint8_t sub_index, osal_uint8_t value_info, ec_coe_sdo_entry_desc_t *desc, 
        osal_uint32_t *error_code) {
    assert(pec != NULL);
    assert(desc != NULL);

    int ret = EC_OK; 

    if ((index & 0xF000) == 0x8000) {
        if (sub_index <= 35) {
            memcpy(desc, &entry_desc_master_0x8nnn[sub_index], sizeof(ec_coe_sdo_entry_desc_t));
        }
    } else if ((index & 0xF000) == 0x9000) {
        if (sub_index <= 32) {
            memcpy(desc, &entry_desc_master_0x9nnn[sub_index], sizeof(ec_coe_sdo_entry_desc_t));
        }
    } else if ((index & 0xF000) == 0xA000) {
        if (sub_index <= 2) {
            memcpy(desc, &entry_desc_master_0xAnnn[sub_index], sizeof(ec_coe_sdo_entry_desc_t));
        }
    } else if (index == 0xF000) {
        if (sub_index < 5) {
            memcpy(desc, &entry_desc_master_0xF000[sub_index], sizeof(ec_coe_sdo_entry_desc_t));
        }
    } else if (index == 0xF002) {
        if (sub_index < 4) {
            memcpy(desc, &entry_desc_master_0xF002[sub_index], sizeof(ec_coe_sdo_entry_desc_t));
        }
    }
    return ret;
}

