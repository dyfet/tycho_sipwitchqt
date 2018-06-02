#!/bin/sh
# Copyright (C) 2017-2018 Tycho Softworks.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

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
