#!/sbin/openrc-run
description="sipwitchqt server"

name="sipwitchqt"
pidfile="/var/run/sipwitchqt.pid"
command="/usr/sbin/sipwitchqt-server"
command_user="sipwitchqt:sipwitchqt"
command_background=true
start_stop_daemon_args="-g sipwitchqt"
extra_started_commands="reload"

depend() {
    need hostname localmount
    provide sip-server
}

reload() {
  ebegin "Reloading ${name}"
  start-stop-daemon --signal HUP --pidfile "${pidfile}"
  eend $?
}

