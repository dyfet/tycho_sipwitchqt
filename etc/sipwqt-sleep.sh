#!/bin/sh
# for /lib/systemd/system-sleep
# or /etc/pm/sleep.d
pid=`ps ax | grep SipWitchQt | grep -v grep | head -1 | cut -d\  -f1` 
case "$1$2" in
suspend|presuspend)
	kill -s SIGUSR1 $pid
	;;
resume|postsuspend)
	(sleep 4 ; kill -s SIGUSR2 $pid) &
	;;
esac
