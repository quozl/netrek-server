set log y
set log y2
set grid
set terminal png size 800,600
set output "rates.png"
set key below
set y2tics
set title "graph of netrek data rate changes, 1988 to 2008"
plot \
"rates" using 1:2 title "server frames per second" with points, \
"rates" using 1:3 title "client updates per second" with steps, \
"rates" using 1:4 axis x1y2 title "typical internet service" with linespoints, \
"rates" using 1:5 title "MAXPLAYER" with steps
