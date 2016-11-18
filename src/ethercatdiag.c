#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include "libethercat/ec.h"

int usage(int argc, char **argv) {
    printf("%s [-p|--propagation-delay]\n", argv[0]);
    return 0;
}

int main(int argc, char **argv) {
    ec_t *pec;
    int ret, slave, i;
    
    ret = ec_open(&pec, "eth1", 90, 1, 1);

    ret = ec_set_state(pec, EC_STATE_INIT);
    ret = ec_set_state(pec, EC_STATE_PREOP);

    if (argc == 1)
        return usage(argc, argv);

    for (i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "-p") == 0) ||
                strcmp(argv[i], "--propagation-delays") == 0) {
            printf("propagation delays for distributed clocks: \n\n");

            printf("ethercat master\n");

            for(slave = 0; slave < pec->slave_cnt; ++slave) {
                int tmp_parent = pec->slaves[slave].parent;
                while (tmp_parent >= 0) {
                    printf("    ");
                    tmp_parent = pec->slaves[tmp_parent].parent;
                }

                printf("|---");
                printf("slave %2d: delay %lld [ns]\n", 
                        slave, pec->slaves[slave].pdelay);
            }
        }
    }

    return 0;
}

