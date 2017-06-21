#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <fcntl.h>
#include "libethercat/ec.h"
#include <stdarg.h>

int usage(int argc, char **argv) {
    printf("%s -i|--interface <intf> [-p|--propagation-delay]\n", argv[0]);
    return 0;
}

// only log level <= 10 
void no_verbose_log(int lvl, void *user, const char *format, ...) {
    if (lvl > 5)
        return;

    va_list ap;
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
};

enum tool_mode {
    mode_undefined,
    mode_read,
    mode_write
};

int main(int argc, char **argv) {
    ec_t *pec;
    int ret, slave = -1, i;

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
            mode = mode_read; 
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
    ec_log_func_user = NULL;
    ec_log_func = &no_verbose_log;

    ret = ec_open(&pec, intf, 90, 1, 1);

    switch (mode) {
        default: 
            break;
        case mode_read: {
            uint8_t bigbuf[2048];
            size_t bigbuf_len = 2048;
            ec_eepromread_len(pec, slave, 0, (uint8_t *)bigbuf, bigbuf_len);

            if (fn) {
                // write to file
                int fd = open(fn, O_CREAT | O_RDWR);
                write(fd, bigbuf, bigbuf_len);
                close(fd);
            } else
                write(1, bigbuf, bigbuf_len);
            break;
        }
        case mode_write: {
            uint8_t bigbuf[2048];
            size_t bigbuf_len = 2048;
            int bytes;
            
            if (fn) {
                int fd = open(fn, O_CREAT | O_RDWR);
                bytes = read(fd, bigbuf, bigbuf_len);
                close(fd);
            } else
                bytes = read(0, bigbuf, bigbuf_len);
            
            ec_eepromwrite_len(pec, slave, 0, (uint8_t *)bigbuf, bigbuf_len);
            break;
        }
    }

    ec_close(pec);

    return 0;
}

