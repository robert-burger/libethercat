#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include "libethercat/ec.h"

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

    for(slave = 0; slave < pec->slave_cnt; ++slave) {
#define print_prefix() {                                    \
        int tmp_parent = pec->slaves[slave].parent;         \
        while (tmp_parent >= 0) {                           \
            printf("|   ");                                 \
            tmp_parent = pec->slaves[tmp_parent].parent;    \
        } }

        ec_slave_t *slv = &pec->slaves[slave];

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
        printf("|         dc support %X, propagation delay %lld [ns]\n", 
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
}

int main(int argc, char **argv) {
    ec_t *pec;
    int ret, slave, i;

    char *intf = NULL;
    int show_propagation_delays = 0;
    
//    ec_log_func_user = NULL;
//    ec_log_func = &no_log;

    for (i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "-i") == 0) ||
                (strcmp(argv[i], "--interface") == 0)) {
            if (++i < argc)
                intf = argv[i];
        } else if ((strcmp(argv[i], "-p") == 0) ||
                (strcmp(argv[i], "--propagation-delays") == 0)) {
            show_propagation_delays = 1; 
        }

    }
    
    if ((argc == 1) || (intf == NULL))
        return usage(argc, argv);

            
    ret = ec_open(&pec, intf, 90, 1, 1);

    if (show_propagation_delays)
        propagation_delays(pec);

    ec_close(pec);

    return 0;
}

