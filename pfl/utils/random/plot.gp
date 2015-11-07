# $Id$

set term png small color picsize 1024 768
set title "PRNG distribution"
set ylabel "#occurances"
set xlabel "psc_random() output value"
set output "random.png"
set grid
set yr [0:]

plot	"data" using 1:2 title "data points", \
	"data" using 1:2 smooth frequency title "bezier" with lines
