#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

#include "libethercat/ec.h"
#include "libethercat/mii.h"

void no_log(int lvl, void *user, const char *format, ...) 
{};


int usage(int argc, char **argv) {
    printf("%s -i|--interface <intf> [-p|--propagation-delay]\n", argv[0]);
    return 0;
}

void propagation_delays(ec_t *pec) {
    int slave, ret = ec_set_state(pec, EC_STATE_INIT);
    ec_dc_config(pec);

    printf("propagation delays for distributed clocks: \n\n");

    printf("ethercat master\n");

    ec_create_pd_groups(pec, 1);
    
    for(slave = 0; slave < pec->slave_cnt; ++slave) {
#define print_prefix() {                                    \
        int tmp_parent = pec->slaves[slave].parent;         \
        while (tmp_parent >= 0) {                           \
            printf("|   ");                                 \
            tmp_parent = pec->slaves[tmp_parent].parent;    \
        } }

        ec_slave_t *slv = &pec->slaves[slave];
        slv->assigned_pd_group = 0;

        print_prefix();
        printf("|---");
        printf("slave %2d: ", 
                slave);

        if ((slv->eeprom.general.name_idx > 0) &&
                (slv->eeprom.general.name_idx <= slv->eeprom.strings_cnt)) {
            printf("%s", slv->eeprom.strings[slv->eeprom.general.name_idx-1]);
        }
        printf("\n");
        

        print_prefix();
        printf("|   ");
        printf("|         dc support %X, propagation delay %d [ns]\n", 
                (slv->features & 0x04) == 0x04, 
                slv->pdelay);

        print_prefix();
        printf("|   ");
        printf("|         link's %d, active ports %d, ptype 0x%X\n",
                slv->link_cnt,
                slv->active_ports,
                slv->ptype);

        print_prefix();
        printf("|   ");
        printf("|         sync manager channel's %d, fmmu channel's %d\n", 
                slv->sm_ch,
                slv->fmmu_ch);      
        
        print_prefix();
        printf("|   ");
        printf("|         auto inc adr %d, fixed addr %d\n", 
                slv->auto_inc_address,
                slv->fixed_address);      
    }
    
    ret = ec_set_state(pec, EC_STATE_PREOP);

    ret = ec_set_state(pec, EC_STATE_SAFEOP);
}

int max_print_level = 0;

// only log level <= 10 
void no_verbose_log(int lvl, void *user, const char *format, ...) {
    if (lvl > max_print_level)
        return;

    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
};

void mii_read(int fd, ec_t *pec, int slave, int phy) {
    int i;
    for (i = 0; i < 0x20; ++i) {
        uint16_t data = 0;
        ec_miiread(pec, slave, phy, i, &data);
        write(fd, &data, sizeof(data));
    }
}

enum tool_mode {
    mode_undefined,
    mode_read,
    mode_write, 
    mode_test
};

int main(int argc, char **argv) {
    ec_t *pec;
    int ret, slave, i, phy = 0;

    char *intf = NULL, *fn = NULL;
    int show_propagation_delays = 0;
    
    long reg = 0, val = 0;
    enum tool_mode mode = mode_undefined;
//    ec_log_func_user = NULL;
//    ec_log_func = &no_log;

    for (i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "-i") == 0) ||
                (strcmp(argv[i], "--interface") == 0)) {
            if (++i < argc)
                intf = argv[i];
//        } else if ((strcmp(argv[i], "-p") == 0) ||
//                (strcmp(argv[i], "--propagation-delays") == 0)) {
//            show_propagation_delays = 1; 
        } else if ((strcmp(argv[i], "-r") == 0) ||
                (strcmp(argv[i], "--read") == 0)) {
            mode = mode_read; 
        } else if ((strcmp(argv[i], "-t") == 0) ||
                (strcmp(argv[i], "--test") == 0)) {
            mode = mode_test; 
        } else if ((strcmp(argv[i], "-w") == 0) ||
                (strcmp(argv[i], "--write") == 0)) {
            mode = mode_write; 
        } else if ((strcmp(argv[i], "-s") == 0) ||
                (strcmp(argv[i], "--slave") == 0)) {
            if (++i < argc)
                slave = atoi(argv[i]);
        } else if ((strcmp(argv[i], "-p") == 0) ||
                (strcmp(argv[i], "--phy-address") == 0)) {
            if (++i < argc)
                phy = atoi(argv[i]);
        } else if ((strcmp(argv[i], "-v") == 0) || 
                (strcmp(argv[i], "--verbose") == 0)) {
            max_print_level = 100;
        } else {
            // interpret as reg:value
            char *tmp = strstr(argv[i], ":");
            if (tmp) {
                char *regstr = strndup(argv[i], (int)(tmp-argv[i]));
                char *valstr = strndup(&tmp[1], (int)(strlen(argv[i]))-strlen(regstr)-1);
                printf("got reg %s, val %s\n", regstr, valstr);
                reg = strtol(regstr, NULL, 16);
                val = strtol(valstr, NULL, 16);
            } else 
                printf("command \"%s\" not understood\n", argv[i]);
        }
    }
    
    if ((argc == 1) || (intf == NULL))
        return usage(argc, argv);

    // use our log function
    ec_log_func_user = NULL;
    ec_log_func = &no_verbose_log;
            

    ret = ec_open(&pec, intf, 90, 1, 1);


    if (show_propagation_delays)
        propagation_delays(pec);

    int j, fd;
    
    if (mode == mode_read) {
        if (fn) {
            fd = open(fn, O_CREAT | O_RDWR);
            mii_read(fd, pec, slave, phy);
            close(fd);
        } else
            mii_read(1, pec, slave, phy);
    } else if (mode == mode_write) {
        uint16_t mii_value = val;
        ec_miiwrite(pec, slave, phy, reg, &mii_value);
    } else if (mode == mode_test) {
        printf("now in test mode...\n");
        while (1) {
            uint16_t tmp, wkc;
            ec_brd(pec, 0, &tmp, 2, &wkc);
        }
    }
        

    ec_close(pec);

    return 0;
}

