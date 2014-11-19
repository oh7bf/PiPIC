# Gnuplot script plotbatt.p to plot battery voltage
# Execute:  gnuplot> load 'plotbatt.p'
#
set title "Battery voltage"
set xdata time
set timefmt "%Y-%m-%d %H:%M:%S"
set format x "%m-%d\n%H:%M"
set xlabel "Date"
set ylabel "U[V]"

# set correct time range here, otherwise nothing is plotted
set xrange ["2014-05-06 06:20":"2014-05-11 10:00"]

# voltint
#plot "201405110951.batt" using 1:3

# volts[V]
#plot "201405110951.batt" using 1:4

# capacity[%]
#plot "201405110951.batt" using 1:5

# empty[h]
#plot "201405110951.batt" using 1:6

# temperature[C]
plot "201405110951.batt" using 1:7

#set term png
#set output "201405110951.png"
#plot "201405110951.txt" using 1:4

