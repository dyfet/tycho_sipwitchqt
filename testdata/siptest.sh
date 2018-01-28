#!/bin/sh
case "$1" in
init)
	# initialize database to run antisipate client for localhost testing
	if test ! -f local.db ; then
		echo "*** no sipwitchqt database found" ; exit 1 ; fi

	../utils/swlite-authorize.rb -c -p testing test
	;;
ping)
	exec sipp 127.0.0.1:4060 -sf ping.xml -p 4050 -m 5 -s 30
	;;
default)
	echo "siptest: unknown test" >&2
	exit 1
	;;
esac
