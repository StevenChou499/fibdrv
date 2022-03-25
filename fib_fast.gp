set title "Fast Doubling Speed"
set xlabel "n^{th} Number"
set ylabel "time(nsec)"
set terminal png font " Times_New_Roman,12 "
set output "Fast_Doubling.png"
set xtics 0 ,10 ,100
set key left 

plot "out" using 4 with linespoints linewidth 2 title "user space", \
"out" using 5 with linespoints linewidth 2 title "kernel space", \
"out" using 6 with linespoints linewidth 2 title "kernel to user", \