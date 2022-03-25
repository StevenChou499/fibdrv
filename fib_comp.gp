set title "Compare Fibonacci Speed"
set xlabel "n^{th} Number"
set ylabel "time(nsec)"
set terminal png font " Times_New_Roman,12 "
set output "Compare_Fib.png"
set xtics 0 ,10 ,100
set key left 

plot "out" using 2 with linespoints linewidth 2 title "Fast kernel space", \
"out" using 5 with linespoints linewidth 2 title "Normal kernel space", \