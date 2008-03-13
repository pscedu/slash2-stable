# $Id$

set term png small color picsize 1024 768
set title "PRNG distribution"
set ylabel "frequency"
set xlabel "output"
set output "random.png"
set grid
#set yr [0:20000]

plot	"data" using 1:2 title "data points", \
	"data" using 1:2 smooth bezier title "bezier" with lines
