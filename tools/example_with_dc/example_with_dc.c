//! ethercat example with distributed clocks
//
/*!
 * author: Robert Burger
 *
 * $Id$
 */

#include <libosal/trace.h>

#include <libethercat/config.h>
#include <libethercat/ec.h>
#include <libethercat/error_codes.h>

#include <stdio.h>
#include <math.h>
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

#include <signal.h>

static volatile sig_atomic_t keep_running = 1;

void sig_handler(int sig) {
  keep_running = 0;
}

void no_log(int lvl, void *user, const char *format, ...) 
{};


int usage(int argc, char **argv) {
    printf("%s -i|--interface <intf> [-v|--verbose] [-p|--prio] [-a|--affinity]\n", argv[0]);
    printf("  -h|--help             Display this help page.\n");
    printf("  -v|--verbose          Set libethercat to print verbose output.\n");
    printf("  -p|--prio             Set base priority for cyclic and rx thread.\n");
    printf("  -a|--affinity         Set CPU affinity for cyclic and rx thread.\n");
    printf("  -c|--clock            Distributed clock master (master/ref).\n");
#if LIBETHERCAT_MBX_SUPPORT_EOE == 1
    printf("  -e|--eoe              Enable EoE for slave network comm.\n");
#endif
    printf("  -f|--cycle-frequency  Specify cycle frequency in [Hz].\n");
    printf("  -b|--busy-wait        Don't sleep, do busy-wait instead.\n");
    printf("  --disable-overlapping Disable LRW data overlapping.\n");
    printf("  --disable-lrw         Disable LRW and use LRD/LWR instead (implies --disable-overlapping).\n");
    return 0;
}

int max_print_level = 10;
osal_uint64_t prog_start_time;

// only log level <= 10 
void no_verbose_log(ec_t *pec, int lvl, const char *format, ...) __attribute__(( format(printf, 3, 4)));
void no_verbose_log(ec_t *pec, int lvl, const char *format, ...) {
    if (lvl > max_print_level)
        return;

    osal_uint64_t print_time = osal_timer_gettime_nsec() - prog_start_time;
    fprintf(stderr, "%7d.%09d -> ", print_time / 1000000000, print_time % 1000000000);

    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
};

static ec_t ec;
static osal_uint64_t cycle_rate = 1000000;
static osal_uint64_t act_cycle_rate = 1000000;
static ec_dc_mode_t dc_mode = dc_mode_master_as_ref_clock;

osal_retval_t (*wait_time)(osal_uint64_t) = osal_sleep_until_nsec;

osal_uint64_t last_sent;
osal_trace_t *tx_start;
osal_trace_t *tx_duration;
osal_trace_t *roundtrip_duration;

osal_size_t bytes_last_sent = 0;

//! Cyclic (high priority realtime) task for sending EtherCAT datagrams.
static osal_bool_t cyclic_task_running = OSAL_FALSE;
static osal_void_t* cyclic_task(osal_void_t* param) {
    ec_t *pec = (ec_t *)param;
    osal_uint64_t abs_timeout = osal_timer_gettime_nsec();
    abs_timeout = (abs_timeout / pec->main_cycle_interval) * pec->main_cycle_interval;
    osal_uint64_t time_start = 0u;

    ec_log(100, "CYCLIC_TASK", "running endless loop, cycle rate is %lu\n", cycle_rate);

    while (cyclic_task_running == OSAL_TRUE) {
        abs_timeout += act_cycle_rate;
        (void)wait_time(abs_timeout);

        last_sent = abs_timeout;
        time_start = osal_trace_point(tx_start);

        // execute one EtherCAT cycle
        ec_send_distributed_clocks_sync_with_rtc(pec, abs_timeout);
        ec_send_process_data(pec);

        // transmit cyclic packets (and also acyclic if there are any)
        hw_tx_high(pec->phw);

        osal_trace_time(tx_duration, osal_timer_gettime_nsec() - time_start);
        bytes_last_sent = ec.phw->bytes_last_sent;
        
        hw_tx_low(pec->phw);
    }

    ec_log(100, "CYCLIC_TASK", "exiting!\n");
}

int main(int argc, char **argv) {
    int ret, slave, i, phy = 0;
    char *intf = NULL, *fn = NULL;
    long reg = 0, val = 0;
    int base_prio = 60;
    int base_affinity = 0x8;
#if LIBETHERCAT_MBX_SUPPORT_EOE == 1
    int eoe = 0;
#endif
    int disable_overlapping = 0;
    int disable_lrw = 0;
    int eeprom_dump = 0;
    int threaded_startup = 0;
    double dc_kp = 10.;
    double dc_ki = 1.;
    ec_t *pec = &ec;

    osal_timer_set_clock_source(CLOCK_MONOTONIC);

    prog_start_time = osal_timer_gettime_nsec();

    for (i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
            return usage(argc, argv);
        } else if ((strcmp(argv[i], "-i") == 0) ||
                (strcmp(argv[i], "--interface") == 0)) {
            if (++i < argc)
                intf = argv[i];
        } else if ((strcmp(argv[i], "-v") == 0) || 
                (strcmp(argv[i], "--verbose") == 0)) {
            max_print_level = 200;
        } 
#if LIBETHERCAT_MBX_SUPPORT_EOE == 1
        else if ((strcmp(argv[i], "-e") == 0) || 
                (strcmp(argv[i], "--eoe") == 0)) {
            eoe = 1;
        } 
#endif
        else if ((strcmp(argv[i], "-b") == 0) || 
                (strcmp(argv[i], "--busy-wait") == 0)) {
            wait_time = osal_busy_wait_until_nsec;
        } else if (strcmp(argv[i], "--disable-overlapping") == 0) {
            disable_overlapping = 1;
        } else if (strcmp(argv[i], "--disable-lrw") == 0) {
            disable_lrw = 1;
        } else if (strcmp(argv[i], "--eeprom-dump") == 0) {
            eeprom_dump = 1;
        } else if (strcmp(argv[i], "--threaded-startup") == 0) {
            threaded_startup = 1;
        } else if ((strcmp(argv[i], "-p") == 0) || 
                (strcmp(argv[i], "--prio") == 0)) {
            if (++i < argc)
                base_prio = strtoul(argv[i], NULL, 10);
        } else if ((strcmp(argv[i], "-f") == 0) || 
                (strcmp(argv[i], "--cycle-frequency") == 0)) {
            if (++i < argc)
                cycle_rate = (1. / strtoul(argv[i], NULL, 10)) * 1E9; // should be [Hz]
        } else if ((strcmp(argv[i], "-a") == 0) || 
                (strcmp(argv[i], "--affinty") == 0)) {
            if (++i < argc) {
                if (argv[i][0] == '0' && (argv[i][1] == 'x' || argv[i][1] == 'X'))
                    base_affinity = strtoul(argv[i], NULL, 16);
                else
                    base_affinity = strtoul(argv[i], NULL, 10);
            }
        } else if ((strcmp(argv[i], "-c") == 0) ||
                (strcmp(argv[i], "--clock") == 0)) {
            if (++i < argc) {
                if (strcmp(argv[i], "master") == 0) {
                    dc_mode = dc_mode_master_as_ref_clock;
                } else {
                    dc_mode = dc_mode_ref_clock;

                    if ((i+1) < argc) {
                        if (argv[i+1][0] != '-') {
                            i++;
                            dc_kp = strtod(argv[i], NULL);

                            if (argv[i+1][0] != '-') {
                                i++;
                                dc_ki = strtod(argv[i], NULL);
                            }
                        }
                    }
                }
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

    int num_samples = 1. / (cycle_rate * 1E-9);
    osal_trace_alloc(&tx_start, num_samples);
    osal_trace_alloc(&tx_duration, num_samples);
    osal_trace_alloc(&roundtrip_duration, num_samples);

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
        ret = hw_device_sock_raw_mmaped_open(&hw_sock_raw_mmaped, pec, intf, base_prio - 1, base_affinity);

        if (ret == 0) {
            phw = &hw_sock_raw_mmaped.common;
        }
    }
#endif

    if (phw == NULL) {
        ec_log(10, "HW_OPEN", "Hardware device layer failure!\n");
        goto hw_exit;
    }

    ret = ec_open(&ec, phw, eeprom_dump);
    if (ret != EC_OK) {
        goto exit;
    }

    ec.threaded_startup = threaded_startup;
    
    ec_set_state(&ec, EC_STATE_INIT);

#if LIBETHERCAT_MBX_SUPPORT_EOE
    if (eoe != 0) {
        osal_uint8_t ip[4] = { 1, 100, 168, 192 };
        ec_configure_tun(&ec, ip);

        osal_uint8_t mac[6] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };
        osal_uint8_t ip_address[4] = { 2, 100, 168, 192 };
        osal_uint8_t subnet[4] = { 0, 255, 255, 255 };
        osal_uint8_t gateway[4] = { 1, 100, 168, 192 };
        osal_uint8_t dns[4] = { 1, 100, 168, 192 };

        // configure slave settings.
        for (int i = 0; i < ec.slave_cnt; ++i) {
            if (ec_mbx_check(&ec, i, EC_EEPROM_MBX_EOE) == EC_OK) {
                ec_slave_set_eoe_settings(&ec, i, mac, ip_address, subnet, gateway, dns, NULL);
                mac[0]++;
                ip_address[0]++;
            }
        }
    }
#endif

    ec_set_state(&ec, EC_STATE_PREOP);

    ec_configure_dc(&ec, cycle_rate, dc_mode, ({
                void anon_cb(void *arg, int num) { 
                    if (dc_mode == dc_mode_ref_clock) {
                        act_cycle_rate = cycle_rate + ec.dc.timer_correction;
                    }
                } &anon_cb; }), NULL);

    int cycle_rate_hz = 1. / (cycle_rate * 1E-9);
    ec.dc.control.kp = dc_kp / cycle_rate_hz;
    ec.dc.control.ki = dc_ki / (cycle_rate_hz * cycle_rate_hz);
    ec.dc.control.diffsum_limit = cycle_rate / 1000.;
    //ec_configure_dc_settling_time(&ec, 50000000000);
//    ec_configure_dc_settling_threshold(&ec, 5000, 10000); // 1000 cycles below 5000ns

    // -----------------------------------------------------------
    // creating process data groups
    ec_create_pd_groups(&ec, 1);
    ec_configure_pd_group(&ec, 0, 1, ({
                void anon_cb(void *arg, int num) { 
                    osal_uint64_t time_end = osal_timer_gettime_nsec();
                    osal_uint64_t time_start = osal_trace_get_last_time(tx_start);

                    osal_trace_time(roundtrip_duration, time_end - time_start);
                } &anon_cb; }), NULL);
        
    ec.pd_groups[0].use_lrw = disable_lrw == 0 ? 1 : 0;
    ec.pd_groups[0].overlapping = disable_overlapping == 0 ? 1 : 0;

    // -----------------------------------------------------------
    // configure slave settings.
    for (int i = 0; i < ec.slave_cnt; ++i) {
        ec.slaves[i].assigned_pd_group = 0;
        ec_slave_set_dc_config(&ec, i, 1, EC_DC_ACTIVATION_REG_SYNC0, cycle_rate, 0, -50000);
    }
        
    cyclic_task_running = OSAL_TRUE;
    osal_task_attr_t cyclic_task_attr = { "cyclic_task", OSAL_SCHED_POLICY_FIFO, base_prio, base_affinity };
    osal_task_t cyclic_task_hdl;
    osal_task_create(&cyclic_task_hdl, &cyclic_task_attr, cyclic_task, &ec);
    ec_set_state(&ec, EC_STATE_SAFEOP);
    ec_set_state(&ec, EC_STATE_OP);

    signal(SIGINT, sig_handler);

    // wait here
    osal_uint64_t tx_timer_med = 0, tx_timer_avg_jit = 0, tx_timer_max_jit = 0;
    osal_uint64_t tx_duration_med = 0, tx_duration_avg_jit = 0, tx_duration_max_jit = 0;
    osal_uint64_t roundtrip_duration_med = 0, roundtrip_duration_avg_jit = 0, roundtrip_duration_max_jit = 0;

    for (;keep_running == 1;) {
        osal_timer_t to;
        osal_timer_init(&to, 10000000000);
        osal_trace_timedwait(tx_duration, &to);

        osal_trace_analyze(tx_start, &tx_timer_med, &tx_timer_avg_jit, &tx_timer_max_jit);
        osal_trace_analyze_rel(tx_duration, &tx_duration_med, &tx_duration_avg_jit, &tx_duration_max_jit);
        osal_trace_analyze_rel(roundtrip_duration, &roundtrip_duration_med, &roundtrip_duration_avg_jit, &roundtrip_duration_max_jit);

        ec_log(10, "MAIN", "rtc_time %" PRIu64 ", dc_time %" PRIu64 "\n", ec.dc.rtc_time, ec.dc.dc_time);

#define to_us(x)    ((double)(x)/1000.)
        if (dc_mode != dc_mode_ref_clock) {
            ec_log(10, "", "=====================================================================================================\n");
            ec_log(10, "Times", "RTC %15.9fs, Last Sent %15.9fs, DC %15.9fs\n", ec.dc.rtc_time/1E9, last_sent/1E9, ec.dc.dc_time/1E9);
            ec_log(10, "Frame", "Length %" PRIu64 " bytes, Time @ 100 MBit/s %7.1fus\n", bytes_last_sent, (10 * 8 * bytes_last_sent) / 1000.);
            ec_log(10, "Mean (Stddev,Maxdev)", "Timer %7.1fus (%4" PRId64 "ns/%4" PRId64 "ns), TX %7.1fus (%4" PRId64 "ns/%4" PRId64 "ns), "
                    "Roundtrip %5.1fus (%4" PRId64 "ns/%4" PRId64 "ns)\n", to_us(tx_timer_med), tx_timer_avg_jit, tx_timer_max_jit, 
                    to_us(tx_duration_med), tx_duration_avg_jit, tx_duration_max_jit, 
                    to_us(roundtrip_duration_med), roundtrip_duration_avg_jit, roundtrip_duration_max_jit);
        } else {
            ec_log(10, "", "=====================================================================================================\n");
            ec_log(10, "Times", "RTC %15.9fs, Last Sent %15.9fs, DC %15.9fs\n", ec.dc.rtc_time/1E9, last_sent/1E9, ec.dc.dc_time/1E9);
            ec_log(10, "Frame", "Length %" PRIu64 " bytes, Time @ 100 MBit/s %7.1fus\n", bytes_last_sent, (10 * 8 * bytes_last_sent) / 1000.);
            ec_log(10, "Mean (Stddev,Maxdev)", "Timer %7.1fus (%4" PRId64 "ns/%4" PRId64 "ns), TX %7.1fus (%4" PRId64 "ns/%4" PRId64 "ns), "
                    "Roundtrip %5.1fus (%4" PRId64 "ns/%4" PRId64 "ns)\n", to_us(tx_timer_med), tx_timer_avg_jit, tx_timer_max_jit, 
                    to_us(tx_duration_med), tx_duration_avg_jit, tx_duration_max_jit, 
                    to_us(roundtrip_duration_med), roundtrip_duration_avg_jit, roundtrip_duration_max_jit);
            ec_log(10, "DC", "Diff %4" PRId64 "ns, diffsum %+7.1fns, cylce_rate %ldns\n", ec.dc.act_diff, ec.dc.control.diffsum, act_cycle_rate);
        }
    }

exit:
    ec_set_state(&ec, EC_STATE_PREOP);
    
    cyclic_task_running = OSAL_FALSE;
    osal_task_join(&cyclic_task_hdl, NULL);
    
    ec_close(&ec);

hw_exit:
    osal_trace_free(tx_start);
    osal_trace_free(tx_duration);
    osal_trace_free(roundtrip_duration);

    return 0;
}

