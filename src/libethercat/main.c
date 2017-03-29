#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include "ec.h"

int main(int argc, char **argv) {
    ec_t *pec;
    ec_open(&pec, "eth0", 10, 0xf);
    ec_close(pec);
    return 0;
}


