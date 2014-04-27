# print battery and temperature info
# this can be added to end of .bashrc
/bin/echo "Battery at $(/bin/cat /var/lib/pipicpowerd/volts) V \
$(/bin/cat /var/lib/pipicpowerd/battlevel)% \
$(/bin/cat /var/lib/pipicpowerd/ophours) h left \
$(/bin/cat /var/lib/pipicpowerd/hoursleft) h to empty \
at $(/bin/cat /var/lib/tmp102d/temperature) °C \
CPU $(/opt/vc/bin/vcgencmd measure_temp)"

