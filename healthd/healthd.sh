#!/bin/bash
#
# $Id: healthd.sh 2162 2008-07-14 14:24:37Z hslmatejf $
#
# healthd --    This is a simple daemon which can be used to alert you in the
#               event of a hardware health monitoring alarm by sending an 
#               kern.alert logging message to syslog(3) facility
#
# To Use  --    Simply start the daemon from a shell (may be backgrounded)
#
# Other details -- Checks status every 60 seconds.  Sends warning every
#                  60 seconds during alarm until the alarm is cleared.
#                  Very low loading on the machine (sleeps almost all the time).
# Requirements -- grep, sensors, bash
#                 (You may need to alter the PATH, but probably not.)
#
# Written & Copyrighten by Philip Edelbrock, 1999.
# revision 10.Aug.2004 by Instrumentation Technologies
# Version: 1.0.0
#

PATH=${PATH}:/opt/bin
T_SLEEP=60
if [ -e /sys/class/i2c-adapter/i2c-0/device/0-0048 ]; then
  TEMP_PATH=/sys/class/i2c-adapter/i2c-0/device
else
  # A different path in kernel 2.6.25.7
  TEMP_PATH=/sys/class/i2c-adapter/i2c-0
fi

while true
do
  touch /tmp/sensors  # watchdog check file
  #set speed back to normal and then check alarms
  # Libera Brilliance ?
  Monitor 0x14000008 > /dev/null
    [ $? == 16 ] && {
    for i in $TEMP_PATH/*/speed
    do
       echo 4500 >$i
    done
  } || {
      sensors -s
  }

  sensors | grep ALARM |  while read
    do
    logger -p kern.alert "$REPLY"
    #if alarms are set turn fans to full speed
    for i in $TEMP_PATH/*/speed
    do
       echo 5500 >$i
    done
  done
  sleep $T_SLEEP
done & 
