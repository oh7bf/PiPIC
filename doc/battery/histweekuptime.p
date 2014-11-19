# Gnuplot script histweekuptime.p to plot weekly uptime.
# Execute:  gnuplot> load 'histweekuptime.p'
#
set title "RPiAOne Weekly Uptime from Total 168 h"
set xlabel "Week 2014"
set ylabel "uptime[h]"

# set correct time range here, otherwise nothing is plotted
#set xrange [28:53]
#set yrange [0:80]

set boxwidth 0.9 absolute
set style fill solid 1.0 border -1
set style histogram clustered gap 1 title  offset character 0, 0, 0
set style data histograms

plot 'weekuptimes.txt' using 2:xticlabels(1)


#set term png
#set output "weekuptimes.png"
#plot 'weekuptimes.txt' using 2:xticlabels(1)


