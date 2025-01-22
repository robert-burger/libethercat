//! ethercat example eeprom tool
/*!
 * author: Robert Burger
 *
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libethercat/config.h"
#include "libethercat/ec.h"

#if LIBETHERCAT_HAVE_UNISTD_H == 1
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>

#if LIBETHERCAT_HAVE_SYS_STAT_H == 1
#include <sys/stat.h>
#endif

#if LIBETHERCAT_HAVE_FCNTL_H == 1
#include <fcntl.h>
#endif

#include <stdarg.h>

#if LIBETHERCAT_BUILD_DEVICE_FILE == 1
#include <libethercat/hw_file.h>
static struct hw_file hw_file;
#endif
#if LIBETHERCAT_BUILD_DEVICE_BPF == 1
#include <libethercat/hw_bpf.h>
static struct hw_bpf hw_bpf;
#endif
#if LIBETHERCAT_BUILD_DEVICE_PIKEOS == 1
#include <libethercat/hw_pikeos.h>
static struct hw_pikeos hw_pikeos;
#endif
#if LIBETHERCAT_BUILD_DEVICE_SOCK_RAW_LEGACY == 1
#include <libethercat/hw_sock_raw.h>
static struct hw_sock_raw hw_sock_raw;
#endif
#if LIBETHERCAT_BUILD_DEVICE_SOCK_RAW_MMAPED == 1
#include <libethercat/hw_sock_raw_mmaped.h>
static struct hw_sock_raw_mmaped hw_sock_raw_mmaped;
#endif

int usage(int argc, char **argv) {
    printf("%s -i|--interface <intf> -s|--slave <nr> [-r|--read] [-w|--write] [-f|--file <filename>]\n", argv[0]);
    return 0;
}

// only log level <= 10 
void no_verbose_log(ec_t *pec, int lvl, const char *format, ...) {
    if (lvl > 10)
        return;

    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
};

enum tool_mode {
    mode_undefined,
    mode_read,
    mode_write
};

struct ec ec;
            
#define bigbuf_len  65536
uint8_t bigbuf[bigbuf_len];

int main(int argc, char **argv) {
    int ret, slave = -1, i;
    int base_prio = 0;
    int base_affinity = 0xF;
    ec_t *pec = &ec;

    char *intf = NULL, *fn = NULL;
    enum tool_mode mode = mode_undefined;

    for (i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "-i") == 0) ||
                (strcmp(argv[i], "--interface") == 0)) {
            if (++i < argc)
                intf = argv[i];
        } else if ((strcmp(argv[i], "-r") == 0) ||
                (strcmp(argv[i], "--read") == 0)) {
            mode = mode_read; 
        } else if ((strcmp(argv[i], "-w") == 0) ||
                (strcmp(argv[i], "--write") == 0)) {
            mode = mode_write; 
        } else if ((strcmp(argv[i], "-f") == 0) ||
                (strcmp(argv[i], "--file") == 0)) {
            if (++i < argc)
                fn = argv[i];
        } else if ((strcmp(argv[i], "-s") == 0) ||
                (strcmp(argv[i], "--slave") == 0)) {
            if (++i < argc)
                slave = atoi(argv[i]);
        }
    }

    if (    (argc == 1)     || 
            (intf == NULL)  || 
            (slave == -1)   || 
            (mode == mode_undefined))
        return usage(argc, argv);

    // use our log function
    pec->ec_log_func_user = NULL;
    pec->ec_log_func = &no_verbose_log;
    struct hw_common *phw = NULL;
            
#if LIBETHERCAT_BUILD_DEVICE_FILE == 1
    if ((intf[0] == '/') || (strncmp(intf, "file:", 5) == 0)) {
        // assume char device -> hw_file
        if (strncmp(intf, "file:", 5) == 0) {
            intf = &intf[5];
        }

        ec_log(10, "HW_OPEN", "Opening interface as device file: %s\n", intf);
        ret = hw_device_file_open(&hw_file, &ec, intf, base_prio - 1, base_affinity);

        if (ret == 0) {
            phw = &hw_file.common;
        }
    }
#endif
#if LIBETHERCAT_BUILD_DEVICE_BPF == 1
    if (strncmp(intf, "bpf:", 4) == 0) {
        intf = &intf[4];

        ec_log(10, "HW_OPEN", "Opening interface as BPF: %s\n", intf);
        ret = hw_device_bpf_open(&hw_bpf, intf);

        if (ret == 0) {
            phw = &hw_bpf.common;
        }
    }
#endif
#if LIBETHERCAT_BUILD_DEVICE_PIKEOS == 1
    if (strncmp(intf, "pikeos:", 7) == 0) {
        intf = &intf[7];

        ec_log(10, "HW_OPEN", "Opening interface as pikeos: %s\n", intf);
        ret = hw_device_pikeos_open(&hw_pikeos, intf, base_prio - 1, base_affinity);

        if (ret == 0) {
            phw = &hw_pikeos.common;
        }
    }
#endif
#if LIBETHERCAT_BUILD_DEVICE_SOCK_RAW_LEGACY == 1
    if (strncmp(intf, "sock-raw:", 9) == 0) {
        intf = &intf[9];
        
        ec_log(10, "HW_OPEN", "Opening interface as SOCK_RAW: %s\n", intf);
        ret = hw_device_sock_raw_open(&hw_sock_raw, &ec, intf, base_prio - 1, base_affinity);

        if (ret == 0) {
            phw = &hw_sock_raw.common;
        }
    }
#endif
#if LIBETHERCAT_BUILD_DEVICE_SOCK_RAW_MMAPED == 1
    if (strncmp(intf, "sock-raw-mmaped:", 16) == 0) {
        intf = &intf[16];

        ec_log(10, "HW_OPEN", "Opening interface as mmaped SOCK_RAW: %s\n", intf);
        ret = hw_device_sock_raw_mmaped_open(&hw_sock_raw_mmaped, &ec, intf, base_prio - 1, base_affinity);

        if (ret == 0) {
            phw = &hw_sock_raw_mmaped.common;
        }
    }
#endif

    ret = ec_open(&ec, phw, 1);
    ec_set_state(&ec, EC_STATE_INIT);

    switch (mode) {
        default: 
            break;
        case mode_read: {
            ec_eepromread_len(&ec, slave, 0, (uint8_t *)bigbuf, bigbuf_len);

            if (fn) {
                // write to file
                int fd = open(fn, O_CREAT | O_RDWR, 0777);
                int ret = write(fd, bigbuf, bigbuf_len);
                
                if (ret == -1) {
                    perror("write returned:");
                }
                
                close(fd);
            } else {
                int ret = write(1, bigbuf, bigbuf_len);

                if (ret == -1) {
                    perror("write returned:");
                }
            }
            break;
        }
        case mode_write: {
            int bytes;
            uint16_t wkc;
            
            if (fn) {
                int fd = open(fn, O_CREAT | O_RDWR, 0777);
                bytes = read(fd, bigbuf, bigbuf_len);
                close(fd);
            } else
                bytes = read(0, bigbuf, bigbuf_len);
            
            if (bytes < 0) {
                ec_log(10, "EEPROM WRITE", "error reading %s\n", fn);
                exit(-1);
            }

            ec_log(10, "EEPROM WRITE", "slave %2d: writing %d bytes\n", slave, bytes);
            ec_eepromwrite_len(&ec, slave, 0, (uint8_t *)bigbuf, bytes);
                
            ec_log(10, "EEPROM WRITE", "slave %2d: try to reset PDI\n", slave);
            osal_uint8_t reset_vals[] = { (osal_uint8_t)'R', (osal_uint8_t)'E', (osal_uint8_t)'S' };
            for (int i = 0; i < 3; ++i) {
                (void)ec_fpwr(&ec, ec.slaves[slave].fixed_address, 0x41, &reset_vals[i], 1, &wkc);
            }
            ec_log(10, "EEPROM WRITE", "slave %2d: try to reset ESC\n", slave);
            for (int i = 0; i < 3; ++i) {
                (void)ec_fpwr(&ec, ec.slaves[slave].fixed_address, 0x40, &reset_vals[i], 1, &wkc);
            }

            break;
        }
    }

    ec_close(&ec);

    return 0;
}

