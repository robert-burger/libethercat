#
# Regular cron jobs for the libethercat package
#
0 4	* * *	root	[ -x /usr/bin/libethercat_maintenance ] && /usr/bin/libethercat_maintenance
