set title "Fast Doubling with clz speed"
set xlabel "n^{th} Number"
set ylabel "time(nsec)"
set terminal png font " Times_New_Roman,12 "
set output "Fast_Doubling_clz.png"
set xtics 0 ,10 ,100
set key left 

plot "out" using 7 with linespoints linewidth 2 title "user space", \
"out" using 8 with linespoints linewidth 2 title "kernel space", \
"out" using 9 with linespoints linewidth 2 title "kernel to user", \