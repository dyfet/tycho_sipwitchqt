#!/bin/sh
#
# PROVIDE: sipwitchqt
# REQUIRE: networking
# KEYWORD:

. /etc/rc.subr

name="sipwitchqt"
rcvar="sipwitchqt_enable"
pidfile="/var/run/${name}.pid"
sipwitchqt_user="sipwitchqt"
sipwitchqt_group="sipwitchqt"
sipqitchqt_command="/usr/local/bin/sipwitchqt-server"
command="/usr/sbin/daemon"
command_args="-P ${pidfile} -r -f ${sipwitchqt_command}"
extra_commands="reload"

load_rc_config $name
run_rc_command "$1"

