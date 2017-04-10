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

#ifdef DEFAULT_NETWORK_SHELL
#include <rtems/console.h>
#include <rtems/shell.h>
#endif

#define RTEMS_BSD_CONFIG_NET_PF_UNIX
#define RTEMS_BSD_CONFIG_NET_IF_LAGG
#define RTEMS_BSD_CONFIG_NET_IF_VLAN
#define RTEMS_BSD_CONFIG_BSP_CONFIG
#define RTEMS_BSD_CONFIG_INIT
#include <machine/rtems-bsd-commands.h>

#include <config.h>


#include "libethercat/ec.h"


void no_log(int lvl, void *user, const char *format, ...) 
{};


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
    
    ret = ec_set_state(pec, EC_STATE_PREOP);

    ret = ec_set_state(pec, EC_STATE_SAFEOP);
}

#include <sys/stat.h>


static void
default_network_set_self_prio(rtems_task_priority prio)
{
	rtems_status_code sc;

	sc = rtems_task_set_priority(RTEMS_SELF, prio, &prio);
	assert(sc == RTEMS_SUCCESSFUL);
}

static void
default_network_ifconfig_hwif0(char *ifname)
{
	int exit_code;
	char *ifcfg[] = {
		"ifconfig",
		ifname,
		"up",
		NULL
	};

	exit_code = rtems_bsd_command_ifconfig(RTEMS_BSD_ARGC(ifcfg), ifcfg);
	assert(exit_code == EX_OK);
}


void configure_network(){
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
  rtems_task_priority prio = RTEMS_MAXIMUM_PRIORITY - 1U;
  rtems_task_set_priority(RTEMS_SELF, prio, &prio);


  fprintf(stderr, " DONE\n");
}

static void Init(rtems_task_argument arg) {
  fprintf(stderr, "Starting ethercatdiag!\n");
#ifdef HAVE_NET_BPF_H
  fprintf(stderr, "Using NET_BPF!\n");
#endif
    ec_t *pec;
    int ret, slave, i;

    char *intf = "cgem0";
    int show_propagation_delays = 0;
    
    configure_network();

    ret = ec_open(&pec, intf, 90, 1, 1);

    if (show_propagation_delays)
        propagation_delays(pec);

    ec_close(pec);
}




/*
 * Configure LibBSD.
 */

#if defined(LIBBSP_I386_PC386_BSP_H)
#define RTEMS_BSD_CONFIG_DOMAIN_PAGE_MBUFS_SIZE (64 * 1024 * 1024)
#elif defined(LIBBSP_POWERPC_QORIQ_BSP_H)
#define RTEMS_BSD_CONFIG_DOMAIN_PAGE_MBUFS_SIZE (32 * 1024 * 1024)
#endif

#define RTEMS_BSD_CONFIG_NET_PF_UNIX
#define RTEMS_BSD_CONFIG_NET_IF_LAGG
#define RTEMS_BSD_CONFIG_NET_IF_VLAN
#define RTEMS_BSD_CONFIG_BSP_CONFIG
#define RTEMS_BSD_CONFIG_INIT

#include <machine/rtems-bsd-config.h>

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_STUB_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_ZERO_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_LIBBLOCK

#define CONFIGURE_USE_IMFS_AS_BASE_FILESYSTEM

#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 32

#define CONFIGURE_MAXIMUM_USER_EXTENSIONS 1

#define CONFIGURE_UNLIMITED_ALLOCATION_SIZE 32
#define CONFIGURE_UNLIMITED_OBJECTS
#define CONFIGURE_UNIFIED_WORK_AREAS

#define CONFIGURE_STACK_CHECKER_ENABLED

#define CONFIGURE_BDBUF_BUFFER_MAX_SIZE (64 * 1024)
#define CONFIGURE_BDBUF_MAX_READ_AHEAD_BLOCKS 4
#define CONFIGURE_BDBUF_CACHE_MEMORY_SIZE (1 * 1024 * 1024)

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_INIT_TASK_STACK_SIZE (32 * 1024)
#define CONFIGURE_INIT_TASK_INITIAL_MODES RTEMS_DEFAULT_MODES
#define CONFIGURE_INIT_TASK_ATTRIBUTES RTEMS_FLOATING_POINT

#define CONFIGURE_INIT

#include <rtems/confdefs.h>

#ifdef DEFAULT_NETWORK_SHELL

#define CONFIGURE_SHELL_COMMANDS_INIT

#include <bsp/irq-info.h>

#include <rtems/netcmds-config.h>

#define CONFIGURE_SHELL_USER_COMMANDS \
  &bsp_interrupt_shell_command, \
  &rtems_shell_BSD_Command, \
  &rtems_shell_HOSTNAME_Command, \
  &rtems_shell_PING_Command, \
  &rtems_shell_ROUTE_Command, \
  &rtems_shell_NETSTAT_Command, \
  &rtems_shell_IFCONFIG_Command, \
  &rtems_shell_TCPDUMP_Command, \
  &rtems_shell_SYSCTL_Command

#define CONFIGURE_SHELL_COMMAND_CPUUSE
#define CONFIGURE_SHELL_COMMAND_PERIODUSE
#define CONFIGURE_SHELL_COMMAND_STACKUSE
#define CONFIGURE_SHELL_COMMAND_PROFREPORT

#define CONFIGURE_SHELL_COMMAND_CP
#define CONFIGURE_SHELL_COMMAND_PWD
#define CONFIGURE_SHELL_COMMAND_LS
#define CONFIGURE_SHELL_COMMAND_LN
#define CONFIGURE_SHELL_COMMAND_LSOF
#define CONFIGURE_SHELL_COMMAND_CHDIR
#define CONFIGURE_SHELL_COMMAND_CD
#define CONFIGURE_SHELL_COMMAND_MKDIR
#define CONFIGURE_SHELL_COMMAND_RMDIR
#define CONFIGURE_SHELL_COMMAND_CAT
#define CONFIGURE_SHELL_COMMAND_MV
#define CONFIGURE_SHELL_COMMAND_RM
#define CONFIGURE_SHELL_COMMAND_MALLOC_INFO
#define CONFIGURE_SHELL_COMMAND_SHUTDOWN

#include <rtems/shellconfig.h>

#endif /* DEFAULT_NETWORK_SHELL */
