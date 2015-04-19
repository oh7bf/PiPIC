#!/usr/bin/gnuplot
# Gnuplot script histweekuptime.p to plot weekly uptime.
# Execute:  gnuplot> load 'histweekuptime.p'
#
set title "RPiAOne Weekly Uptime from Total 168 h"
set xlabel "Week 2014 - 2015"
set ylabel "uptime[h]"

# set correct time range here, otherwise nothing is plotted
#set xrange [28:53]
#set yrange [0:80]

set boxwidth 0.9 absolute
set style fill solid 1.0 border -1
set style histogram clustered gap 1 title  offset character 0, 0, 0
set style data histograms
set xtics rotate by 90

plot '/tmp/weekuptimes.tx' using 3:xticlabels(2)

set term png
set output "/tmp/weekuptimes.png"
plot '/tmp/weekuptimes.tx' using 3:xticlabels(2)


