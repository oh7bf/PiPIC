#! /bin/sh
# /etc/init.d/pipicpowerd
#
### BEGIN INIT INFO
# Provides:          pipicpowerd
# Required-Start:    $syslog $time $remote_fs
# Required-Stop:     $syslog $time $remote_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Control and monitor Raspberry Pi power supply 
### END INIT INFO
#
# Author:      Jaakko Koivuniemi <oh7bf@sral.fi>	
#

NAME=pipicpowerd
PATH=/bin:/usr/bin:/sbin:/usr/sbin
DAEMON=/usr/sbin/pipicpowerd
PIDFILE=/var/run/pipicpowerd.pid

test -x $DAEMON || exit 0

. /lib/lsb/init-functions

case "$1" in
  start)
	log_daemon_msg "Starting daemon" $NAME
        start_daemon -p $PIDFILE $DAEMON
        log_end_msg $?     
    ;;
  stop)
	log_daemon_msg "Stopping daemon" $NAME
        killproc -p $PIDFILE $DAEMON
        log_end_msg $?	
    ;;
  force-reload|restart)
    $0 stop
    $0 start
    ;;
  status)
    status_of_proc -p $PIDFILE $DAEMON $NAME && exit 0 || exit $? 
    ;;
  *)
    echo "Usage: /etc/init.d/pipicpowerd {start|stop|restart|force-reload|status}"
    exit 1
    ;;
esac

exit 0
