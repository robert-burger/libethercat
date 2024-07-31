#include <libethercat/config.h>

#include <stdio.h>
#include <inttypes.h>

#if LIBETHERCAT_HAVE_UNISTD_H == 1
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if LIBETHERCAT_HAVE_SYS_STAT_H == 1
#include <sys/stat.h>
#endif

#if LIBETHERCAT_HAVE_FCNTL_H == 1
#include <fcntl.h>
#endif

#include <stdarg.h>

#include "libethercat/ec.h"
#include "libethercat/mii.h"

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

void no_log(int lvl, void *user, const char *format, ...) 
{};


int usage(int argc, char **argv) {
    printf("%s -i|--interface <intf> [-v|--verbose] [-r|--read] [-w|--write] -s|--slave <nr> [-p|--password <pw>] from to\n", argv[0]);
    printf("  -i|--interface <intf>     EtherCAT master interface to use.\n");
    printf("  -v|--verbose              Set libethercat to print verbose output.\n");
    printf("  -r|--read                 Tool read/upload mode.\n");
    printf("  -w|--write                Tool write/download mode.\n");
    printf("  -s|--slave <nr>           Slave number for upload/download\n");
    printf("  -p|--password <pw>        File password (32-bit unsigned number, either decimal or hex (e.g. 0x12345678))\n");
    return 0;
}

int max_print_level = 10;
static char message[512];
int new_line = 0;

// only log level <= 10 
void no_verbose_log(int lvl, void *user, const char *format, ...) __attribute__(( format(printf, 3, 4)));
void no_verbose_log(int lvl, void *user, const char *format, ...) {
    if (lvl > max_print_level)
        return;

    va_list ap;
    va_start(ap, format);
    if (    (strstr(format, "sending file offset") != NULL) ||
            (strstr(format, "retrieving file offset") != NULL) )
    {
        vsnprintf(&message[1], 511, format, ap);
        message[0] = '\r';
        message[strlen(message)-1] = '\0';
        fprintf(stderr, "%s", message);
        new_line = 1;
    } else {
        if (new_line == 1) {
            new_line = 0;
            fprintf(stderr, "\n");
        }

        vfprintf(stderr, format, ap);
    }
    va_end(ap);
};

enum tool_mode {
    mode_undefined,
    mode_read,
    mode_write, 
};
    
ec_t ec;

int main(int argc, char **argv) {
    int ret, slave, i, phy = 0;
    int base_prio = 0;
    int base_affinity = 0xF;

    char *intf = NULL, *first_fn = NULL, *second_fn = NULL;
    uint32_t password = 0;
    int show_propagation_delays = 0;
    
    long reg = 0, val = 0;
    enum tool_mode mode = mode_undefined;
//    ec_log_func_user = NULL;
//    ec_log_func = &no_log;

    for (i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "-i") == 0) || (strcmp(argv[i], "--interface") == 0)) {
            if (++i < argc) {
                intf = argv[i];
            }
        } else if ((strcmp(argv[i], "-r") == 0) || (strcmp(argv[i], "--read") == 0)) {
            mode = mode_read; 
        } else if ((strcmp(argv[i], "-w") == 0) || (strcmp(argv[i], "--write") == 0)) {
            mode = mode_write; 
        } else if ((strcmp(argv[i], "-s") == 0) || (strcmp(argv[i], "--slave") == 0)) {
            if (++i < argc) {
                slave = atoi(argv[i]);
            }
        } else if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--password") == 0)) {
            if (++i < argc) {
                if (argv[i][0] == '0' && (argv[i][1] == 'x' || argv[i][1] == 'X')) {
                    password = strtoul(argv[i], NULL, 16);
                } else {
                    password = strtoul(argv[i], NULL, 10);
                }
            }
        } else if ((strcmp(argv[i], "-v") == 0) || 
                (strcmp(argv[i], "--verbose") == 0)) {
            max_print_level = 100;
        } else if ((strcmp(argv[i], "-q") == 0) || 
                (strcmp(argv[i], "--quiet") == 0)) {
            max_print_level = 1;
        } else if ((argv[i][0] != '-') && (first_fn == NULL)) {
            first_fn = argv[i];
        } else if ((argv[i][0] != '-') && (second_fn == NULL)) {
            second_fn = argv[i];
        }
    }
    
    if ((argc == 1) || (intf == NULL))
        return usage(argc, argv);

    // use our log function
    ec_log_func_user = NULL;
    ec_log_func = &no_verbose_log;
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
        ret = hw_device_sock_raw_mmaped_open(&hw_sock_raw_mmaped, intf);

        if (ret == 0) {
            phw = &hw_sock_raw_mmaped.common;
        }
    }
#endif
            
    ret = ec_open(&ec, phw, 1);

    ec_set_state(&ec, EC_STATE_INIT);
    ec_set_state(&ec, EC_STATE_BOOT);

    osal_uint8_t *file_data;
    osal_size_t file_data_len;
    const osal_char_t *error_message;

    if (mode == mode_read) {
        osal_uint8_t *string = NULL;
        osal_size_t fsize = 0;
        const osal_char_t *err;

        ec_foe_read(&ec, slave, password, first_fn, &string, &fsize, &err);

        if (string != NULL) {        
            if (second_fn == NULL) {
                write(1, string, fsize);
                write(1, "\n", 1);
            } else {
                if (strcmp(second_fn, ".") == 0) { second_fn = first_fn; }
                FILE *f = fopen(second_fn, "wb");
                fwrite(string, fsize, 1, f);
                fclose(f);

                free(string);
            }
        } else {
            printf("file read was not successfull: data %p, fsize %" PRIu64 ", err %s\n", string, fsize, err);
        }
    } else if (mode == mode_write) {
        FILE *f = fopen(first_fn, "rb");
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);  /* same as rewind(f); */

        char *string = malloc(fsize + 1);
        int local_ret = fread(string, fsize, 1, f);
        if (local_ret == 0) {
            printf("waring got 0 bytes from file!\n");
        }
        fclose(f);

        string[fsize] = 0;

        file_data = (osal_uint8_t *)&string[0];
        file_data_len = fsize;

        ec_foe_write(&ec, slave, password, second_fn, file_data, file_data_len, &error_message);
    }

    ec_close(&ec);

    return 0;
}

