#include <libethercat/config.h>

#include <stdio.h>
#include <inttypes.h>

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

#include "libethercat/ec.h"
#include "libethercat/mii.h"
#include "libethercat/hw_file.h"

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
    
struct hw_file hw_file;
ec_t ec;
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int ret, slave, i, phy = 0;

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
            
    hw_device_file_open(&hw_file, &ec, intf, 90, 1);
    struct hw_common *phw = &hw_file.common;
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
            if (strcmp(second_fn, ".") == 0) { second_fn = first_fn; }
            FILE *f = fopen(second_fn, "wb");
            fwrite(string, fsize, 1, f);
            fclose(f);

            free(string);
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

