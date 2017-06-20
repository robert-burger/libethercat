
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


#include <sys/stat.h>
#include <sys/socket.h>

#include <net/if.h>

#include <assert.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <machine/rtems-bsd-commands.h>

#include <rtems.h>
#include <rtems/printer.h>
#include <rtems/stackchk.h>
#include <rtems/bsd/bsd.h>

#include <rtems/console.h>
#include <rtems/shell.h>
#include <time.h>
#include <libethercat/dc.h>

#include "rtems_config.h"


#include "libethercat/ec.h"
#include "libethercat/dc.h"


void no_log(int lvl, void *user, const char *format, ...) {};

sem_t startupTrig;

static void startPeriodicTask(ec_t *pEc);

void handleEC(ec_t *pEc);

void propagation_delays(ec_t *pec) {
    pec->eeprom_log = 0;
    pec->threaded_startup = 0;
    
    int slave, ret = ec_set_state(pec, EC_STATE_INIT);
    
    
    pec->dc.mode = dc_mode_master_clock;
    pec->dc.act_diff = 1;
//    ec_dc_config(pec);

    pec->dc.timer_override = 333333;
    
    ret = ec_set_state(pec, EC_STATE_PREOP);

    pec->dc.offset_compensation = 2;
    pec->dc.offset_compensation_max = 10000000;
    pec->dc.timer_override = 333333;
    

    
    printf("propagation delays for distributed clocks: \n\n");

    printf("ethercat master\n");

    ec_create_pd_groups(pec, 1);

    for (slave = 0; slave < pec->slave_cnt; ++slave) {
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
            printf("%s", slv->eeprom.strings[slv->eeprom.general.name_idx - 1]);
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

        slv->dc.use_dc = 1;
        slv->dc.type = 0;
        slv->dc.cycle_time_0 = 333333;
        slv->dc.cycle_time_1 = 0;
        slv->dc.cycle_shift = -40000;

///        ec_dc_sync0(pec, slave, 1, 333333, 0);
    }

    pec->dc.act_diff = 0;
  
    pec->tx_sync = 0;

    sem_init(&startupTrig, 0, 0);
    startPeriodicTask(pec);
    sem_wait(&startupTrig);
    ret = ec_set_state(pec, EC_STATE_SAFEOP);    
    ret = ec_set_state(pec, EC_STATE_OP);
    
}

#include <sys/stat.h>
#include <time.h>
//:quad_t :qecvt()

static void
default_network_set_self_prio(rtems_task_priority prio) {
    rtems_status_code sc;

    sc = rtems_task_set_priority(RTEMS_SELF, prio, &prio);
    assert(sc == RTEMS_SUCCESSFUL);
}

static void
default_network_ifconfig_hwif0(char *ifname) {
    fprintf(stderr, "START IFCONFIG\n");
    int exit_code;
    char *ifcfg[] = {
            "ifconfig",
            ifname,
            "up",
            NULL
    };

    exit_code = rtems_bsd_command_ifconfig(RTEMS_BSD_ARGC(ifcfg), ifcfg);
    assert(exit_code == EX_OK);
    fprintf(stderr, "END IFCONFIG (%d)\n", exit_code);
}


void configure_network() {
    fprintf(stderr, "Initializing Network...");
    rtems_status_code sc;
    char *ifname = "cgem0";

    /*
     * Default the syslog priority to 'debug' to aid developers.
     */
    rtems_bsd_setlogpriority("debug");

#ifdef DEFAULT_EARLY_INITIALIZATION
    early_initialization();
#endif

    /* Let other tasks run to complete background work */
    default_network_set_self_prio(RTEMS_MAXIMUM_PRIORITY - 1U);

    rtems_bsd_initialize();

    /* Let the callout timer allocate its resources */
    sc = rtems_task_wake_after(2);
    assert(sc == RTEMS_SUCCESSFUL);

    default_network_ifconfig_hwif0(ifname);

    rtems_bsd_setlogpriority("debug");

    /* Let other tasks run to complete background work */
    //rtems_task_priority prio = RTEMS_MAXIMUM_PRIORITY - 1U;
    //rtems_task_set_priority(RTEMS_SELF, prio, &prio);


    fprintf(stderr, " DONE\n");
}


/**
 * set_normalized_timespec - set timespec sec and nsec parts and normalize
 *
 * @ts:		pointer to timespec variable to be set
 * @sec:	seconds to set
 * @nsec:	nanoseconds to set
 *
 * Set seconds and nanoseconds field of a timespec variable and
 * normalize to the timespec storage format
 *
 * Note: The tv_nsec part is always in the range of
 *	0 <= tv_nsec < NSEC_PER_SEC
 * For negative values only the tv_sec field is negative !
 */
#define NSEC_PER_SEC 1000000000
void set_normalized_timespec(struct timespec *ts, time_t sec, int64_t nsec)
{
    while (nsec >= NSEC_PER_SEC) {
        /*
         * The following asm() prevents the compiler from
         * optimising this loop into a modulo operation. See
         * also __iter_div_u64_rem() in include/linux/time.h
         */
        asm("" : "+rm"(nsec));
        nsec -= NSEC_PER_SEC;
        ++sec;
    }
    while (nsec < 0) {
        asm("" : "+rm"(nsec));
        nsec += NSEC_PER_SEC;
        --sec;
    }
    ts->tv_sec = sec;
    ts->tv_nsec = nsec;
}

void timespec_add(struct timespec* a, uint64_t sec, uint64_t nsec) {
    set_normalized_timespec(a, a->tv_sec + sec, a->tv_nsec + nsec);
}

struct timespec timespec_sub(struct timespec a, struct timespec b) {
    struct timespec ret;
    set_normalized_timespec(&ret, a.tv_sec - b.tv_sec, a.tv_nsec - b.tv_nsec);
    return ret;
}


rtems_task Periodic_task(rtems_task_argument arg) {
    ec_t* pEc = (ec_t *) arg;
    fprintf(stderr, "Periodic_task().\n");
    rtems_name name;
    rtems_id period;
    rtems_status_code status;

    name = rtems_build_name('P', 'E', 'R', 'D');

    
    status = rtems_rate_monotonic_create(name, &period);
    if (status != RTEMS_SUCCESSFUL) {
        fprintf(stderr, "rtems_monotonic_create failed with status of %d.\n", status);
        exit(1);
    }
    
    sem_post(&startupTrig);
    int brCount = 0;
    int lastPrint = 0;
    uint64_t loopCnt = 0;
    while (1) {
        if (rtems_rate_monotonic_period(period, loopCnt%111==0 ? 10 : 9) == RTEMS_TIMEOUT) {
            brCount++;
        }
        if(loopCnt++%15000 == 0 && lastPrint != brCount) {
            fprintf(stderr, "REALTIME VIOLATIONS: %d\n", brCount);
            lastPrint = brCount;
        }
         
        handleEC(pEc);
    }

    status = rtems_rate_monotonic_delete(period);
    if (status != RTEMS_SUCCESSFUL) {
        fprintf(stderr, "rtems_rate_monotonic_delete failed with status of %d.\n", status);
        exit(1);
    }
    status = rtems_task_delete(RTEMS_SELF);    /* should not return */
    fprintf(stderr, "rtems_task_delete returned with status of %d.\n", status);
    
    exit(1);
}

void handleEC(ec_t *pEc) {
    //fprintf(stderr, "handleEC()\n");
    ec_send_process_data_group(pEc, 0);
    ec_send_distributed_clocks_sync(pEc);
    hw_tx(pEc->phw);
    ec_timer_t timeout;
    ec_timer_init(&timeout, 1000000000);
    ec_receive_process_data_group(pEc, 0, &timeout);
    ec_timer_init(&timeout, 1000000000);
    ec_receive_distributed_clocks_sync(pEc, &timeout);
}

static void startPeriodicTask(ec_t *pEc) {
    fprintf(stderr, "startPeriodicTask().\n");
    rtems_id id;
    rtems_status_code sc = rtems_task_create(
		rtems_build_name('C', 'Y', 'C', 'L'),
		RTEMS_MAXIMUM_PRIORITY - 10,
		2 * RTEMS_MINIMUM_STACK_SIZE,
		RTEMS_DEFAULT_MODES,
		RTEMS_FLOATING_POINT,
		&id
	);
    assert(sc == RTEMS_SUCCESSFUL);

	sc = rtems_task_start(id, Periodic_task, pEc);
	assert(sc == RTEMS_SUCCESSFUL);
}


void testTimes(){
    static const int N = 100;
    ec_timer_t t[N];
    for (int i=0; i<N; ++i){
        ec_timer_gettime(&t[i]);
    }
    for (int i=0; i<N; ++i){
        fprintf(stderr, "Time[%d]: %lld:%lld\n", i, t[i].sec, t[i].nsec);
    }
}

rtems_task Init(rtems_task_argument arg) {
    testTimes();
    fprintf(stderr, "Starting ethercatdiag!\n");
#ifdef HAVE_NET_BPF_H
    fprintf(stderr, "Using BPF!\n");
#endif
    ec_t *pec;
    int ret, slave, i;

    char *intf = "cgem0";
    int show_propagation_delays = 1;

    configure_network();

    rtems_status_code sc;
    sc = rtems_shell_init(
            "SHLL",
            32 * 1024,
            1,
            CONSOLE_DEVICE_NAME,
            false,
            false,
            NULL
    );

    struct timespec ts = {3, 0};
    nanosleep(&ts, NULL);
    ret = ec_open(&pec, intf, 90, 1, 1);

    if (show_propagation_delays)
        propagation_delays(pec);
    
    rtems_task_delete(RTEMS_SELF);
    //ec_close(pec);
}


