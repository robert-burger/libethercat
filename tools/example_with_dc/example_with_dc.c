#include <libethercat/config.h>
#include <libethercat/ec.h>
#include <libethercat/error_codes.h>

#include <stdio.h>
#include <math.h>

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


void no_log(int lvl, void *user, const char *format, ...) 
{};


int usage(int argc, char **argv) {
    printf("%s -i|--interface <intf> [-v|--verbose] [-p|--prio] [-a|--affinity]\n", argv[0]);
    printf("  -v|--verbose      Set libethercat to print verbose output.\n");
    printf("  -p|--prio         Set base priority for cyclic and rx thread.\n");
    printf("  -a|--affinity     Set CPU affinity for cyclic and rx thread.\n");
    return 0;
}

int max_print_level = 100;

// only log level <= 10 
void no_verbose_log(int lvl, void *user, const char *format, ...) __attribute__(( format(printf, 3, 4)));
void no_verbose_log(int lvl, void *user, const char *format, ...) {
    if (lvl > max_print_level)
        return;

    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
};

static ec_t ec;
static osal_uint64_t cycle_rate = 1000000;

static osal_binary_semaphore_t duration_tx_sync;
#define DURATION_TX_COUNT   1000
static int duration_tx_pos   = 0;
static osal_uint64_t start_tx_in_ns[DURATION_TX_COUNT];
static osal_uint64_t duration_tx_in_ns[DURATION_TX_COUNT];

static int duration_round_trip_pos = 0;
static osal_uint64_t duration_round_trip_in_ns[DURATION_TX_COUNT];

//! Cyclic (high priority realtime) task for sending EtherCAT datagrams.
static osal_bool_t cyclic_task_running = OSAL_FALSE;
static osal_void_t* cyclic_task(osal_void_t* param) {
    ec_t *pec = (ec_t *)param;
    osal_uint64_t abs_timeout = osal_timer_gettime_nsec();
    osal_uint64_t time_start = 0u;
    osal_uint64_t time_end = 0u;

    no_verbose_log(0, NULL, "cyclic_task: running endless loop\n");

    while (cyclic_task_running == OSAL_TRUE) {
        abs_timeout += cycle_rate;
        osal_sleep_until_nsec(abs_timeout);

        time_start = osal_timer_gettime_nsec();
        start_tx_in_ns[duration_tx_pos] = time_start;

        // execute one EtherCAT cycle
        ec_send_process_data(pec);
        ec_send_distributed_clocks_sync(pec);
        ec_send_brd_ec_state(pec);

        // transmit cyclic packets (and also acyclic if there are any)
        hw_tx(&pec->hw);

        time_end = osal_timer_gettime_nsec();
        duration_tx_in_ns[duration_tx_pos++] = time_end - time_start;
        if (duration_tx_pos >= DURATION_TX_COUNT) {
            duration_tx_pos = 0;
            osal_binary_semaphore_post(&duration_tx_sync);
        }
    }

    no_verbose_log(0, NULL, "cyclic_task: exiting!\n");
}

int main(int argc, char **argv) {
    int ret, slave, i, phy = 0;
    char *intf = NULL, *fn = NULL;
    long reg = 0, val = 0;
    int base_prio = 60;
    int base_affinity = 0x8;

    for (i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "-i") == 0) ||
                (strcmp(argv[i], "--interface") == 0)) {
            if (++i < argc)
                intf = argv[i];
        } else if ((strcmp(argv[i], "-v") == 0) || 
                (strcmp(argv[i], "--verbose") == 0)) {
            max_print_level = 100;
        } else if ((strcmp(argv[i], "-p") == 0) || 
                (strcmp(argv[i], "--prio") == 0)) {
            if (++i < argc)
                base_prio = strtoul(argv[i], NULL, 10);
        } else if ((strcmp(argv[i], "-a") == 0) || 
                (strcmp(argv[i], "--affinty") == 0)) {
            if (++i < argc) {
                if (argv[i][0] == '0' && (argv[i][1] == 'x' || argv[i][1] == 'X'))
                    base_affinity = strtoul(argv[i], NULL, 16);
                else
                    base_affinity = strtoul(argv[i], NULL, 10);
            }
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
            
    ret = ec_open(&ec, intf, base_prio - 1, base_affinity, 1);
    
    ec_set_state(&ec, EC_STATE_INIT);

    osal_uint8_t ip[4] = { 1, 100, 168, 192 };
    ec_configure_tun(&ec, ip);
    
    osal_uint8_t mac[6] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };
    osal_uint8_t ip_address[4] = { 2, 100, 168, 192 };
    osal_uint8_t subnet[4] = { 0, 255, 255, 255 };
    osal_uint8_t gateway[4] = { 1, 100, 168, 192 };
    osal_uint8_t dns[4] = { 1, 100, 168, 192 };
    
    // configure slave settings.
    for (int i = 0; i < ec.slave_cnt; ++i) {
        if (ec_mbx_check(&ec, slave, EC_EEPROM_MBX_EOE) == EC_OK) {
            ec_slave_set_eoe_settings(&ec, i, mac, ip_address, subnet, gateway, dns, NULL);
            mac[0]++;
            ip_address[0]++;
        }
    }

    ec_set_state(&ec, EC_STATE_PREOP);

    ec_configure_dc(&ec, cycle_rate, dc_mode_master_as_ref_clock, ({
                void anon_cb(void *arg, int num) { 
                    osal_uint64_t time_end = osal_timer_gettime_nsec();
                    osal_uint64_t time_start = duration_tx_pos == 0 ? start_tx_in_ns[DURATION_TX_COUNT-1] : start_tx_in_ns[duration_tx_pos-1];
                    //osal_uint64_t time_start = start_tx_in_ns[duration_tx_pos];//duration_tx_pos == 0 ? start_tx_in_ns[DURATION_TX_COUNT-1] : start_tx_in_ns[duration_tx_pos-1];

                    duration_round_trip_in_ns[duration_round_trip_pos++] = time_end - time_start;
                    if (duration_round_trip_pos >= DURATION_TX_COUNT) {
                        duration_round_trip_pos = 0;
                    }
                } &anon_cb; }), NULL);

    // -----------------------------------------------------------
    // creating process data groups
    ec_create_pd_groups(&ec, 1);
    ec_configure_pd_group(&ec, 0, 1, NULL, NULL);
        
    // -----------------------------------------------------------
    // configure slave settings.
    for (int i = 0; i < ec.slave_cnt; ++i) {
        ec.slaves[i].assigned_pd_group = 0;
        ec_slave_set_dc_config(&ec, i, 1, 0, 1000000, 0, 0);
    }

    osal_binary_semaphore_init(&duration_tx_sync, NULL);
    cyclic_task_running = OSAL_TRUE;
    osal_task_attr_t cyclic_task_attr = { "cyclic_task", 0, base_prio, base_affinity };
    osal_task_t cyclic_task_hdl;
    osal_task_create(&cyclic_task_hdl, &cyclic_task_attr, cyclic_task, &ec);

    ec_set_state(&ec, EC_STATE_SAFEOP);
    ec_set_state(&ec, EC_STATE_OP);

    osal_binary_semaphore_wait(&duration_tx_sync);

    // wait here
    osal_uint64_t timer_tx_sum = 0;
    osal_uint64_t timer_tx_med = 0;
    osal_uint64_t timer_tx_avg_jit = 0;
    osal_uint64_t timer_tx_max_jit = 0;

    osal_uint64_t duration_tx_sum = 0;
    osal_uint64_t duration_tx_med = 0;
    osal_uint64_t duration_tx_avg_jit = 0;
    osal_uint64_t duration_tx_max_jit = 0;

    osal_uint64_t duration_round_trip_sum = 0;
    osal_uint64_t duration_round_trip_med = 0;
    osal_uint64_t duration_round_trip_avg_jit = 0;
    osal_uint64_t duration_round_trip_max_jit = 0;

    osal_uint64_t timer_intervals[DURATION_TX_COUNT];
	for (;;) {
        osal_binary_semaphore_wait(&duration_tx_sync);

        timer_tx_avg_jit = 0;
        timer_tx_max_jit = 0;
        timer_tx_sum = 0;
        for (int i = 0; i < DURATION_TX_COUNT-1; ++i) {
            timer_intervals[i] = start_tx_in_ns[i+1] - start_tx_in_ns[i];
            timer_tx_sum += timer_intervals[i];
        }

        timer_tx_med = timer_tx_sum / (DURATION_TX_COUNT-1);
        
        for (int i = 0; i < DURATION_TX_COUNT-1; ++i) {
            osal_int64_t dev = timer_tx_med - timer_intervals[i];
            if (dev < 0) { dev *= -1; }
            if (dev > timer_tx_max_jit) { timer_tx_max_jit = dev; }

            timer_tx_avg_jit += (dev * dev);
        }
        
        timer_tx_avg_jit = sqrt(timer_tx_avg_jit/(DURATION_TX_COUNT-1));

        duration_tx_avg_jit = 0;
        duration_tx_max_jit = 0;
        duration_tx_sum = 0;
        for (int i = 0; i < DURATION_TX_COUNT; ++i) {
            duration_tx_sum += duration_tx_in_ns[i];
        }

        duration_tx_med = duration_tx_sum / DURATION_TX_COUNT;
        
        for (int i = 0; i < DURATION_TX_COUNT; ++i) {
            osal_int64_t dev = duration_tx_med - duration_tx_in_ns[i];
            if (dev < 0) { dev *= -1; }
            if (dev > duration_tx_max_jit) { duration_tx_max_jit = dev; }

            duration_tx_avg_jit += (dev * dev);
        }
        
        duration_tx_avg_jit = sqrt(duration_tx_avg_jit/DURATION_TX_COUNT);

        duration_round_trip_avg_jit = 0;
        duration_round_trip_max_jit = 0;
        duration_round_trip_sum = 0;
        for (int i = 0; i < DURATION_TX_COUNT; ++i) {
            duration_round_trip_sum += duration_round_trip_in_ns[i];
        }

        duration_round_trip_med = duration_round_trip_sum / DURATION_TX_COUNT;
        
        for (int i = 0; i < DURATION_TX_COUNT; ++i) {
            osal_int64_t dev = duration_round_trip_med - duration_round_trip_in_ns[i];
            if (dev < 0) { dev *= -1; }
            if (dev > duration_round_trip_max_jit) { duration_round_trip_max_jit = dev; }

            duration_round_trip_avg_jit += (dev * dev);
        }
        
        duration_round_trip_avg_jit = sqrt(duration_round_trip_avg_jit/DURATION_TX_COUNT);
        
#define to_us(x)    ((double)(x)/1000.)
        no_verbose_log(0, NULL, 
                "Timer %+9.3fus (jitter avg %+7.3fus, max %+7.3fus), "
                "Duration %+7.3fus (jitter avg %+7.3fus, max %+7.3fus), "
                "Round trip %+7.3fus (jitter avg %+7.3fus, max %+7.3fus)\n", 
                to_us(timer_tx_med), 
                to_us(timer_tx_avg_jit), 
                to_us(timer_tx_max_jit), 
                to_us(duration_tx_med), 
                to_us(duration_tx_avg_jit), 
                to_us(duration_tx_max_jit), 
                to_us(duration_round_trip_med),
                to_us(duration_round_trip_avg_jit), 
                to_us(duration_round_trip_max_jit));
	}

    osal_task_join(&cyclic_task_hdl, NULL);

    int j, fd;

    ec_close(&ec);

    return 0;
}

