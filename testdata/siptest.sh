#!/bin/sh
wd=`dirname $0`
cd $wd
test=${1:-ping}
case "$test" in
ping)
	exec sipp 127.0.0.1:4060 -sf ping.xml -p 4050 -m 5 -s 30
	;;
*)
	echo "siptest: $1: unknown test" >&2
	exit 1
	;;
esac
