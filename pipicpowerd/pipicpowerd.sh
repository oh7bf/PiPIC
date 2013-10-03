#! /bin/sh
### BEGIN INIT INFO
# Provides:          pipicpowerd
# Required-Start:    $syslog $time $remote_fs
# Required-Stop:     $syslog $time $remote_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Control and monitor Raspberry Pi power supply 
### END INIT INFO
#
# Author:      Jaakko.Koivuniemi <oh7bf@sral.fi>	
#

NAME=pipicpowerd
PATH=/bin:/usr/bin:/sbin:/usr/sbin
DAEMON=/usr/sbin/pipicpowerd
PIDFILE=/var/run/pipicpowerd.pid

test -x $DAEMON || exit 0

. /lib/lsb/init-functions

case "$1" in
  start)
	echo -n "Starting daemon: " $NAME
	start-stop-daemon --start --quiet --pidfile $PIDFILE --exec $DAEMON
	echo "."
        ;;
  stop)
	echo -n "Stopping daemon: " $NAME
        start-stop-daemon --stop --quiet --exec $DAEMON
	echo "."
        ;;
  force-reload|restart)
    $0 stop
    $0 start
    ;;
  status)
    /bin/ps xa|grep "$DAEMON" | grep -v "grep"
    ;;
  *)
    echo "Usage: /etc/init.d/pipicpowerd {start|stop|restart|force-reload|status}"
    exit 1
    ;;
esac

exit 0
