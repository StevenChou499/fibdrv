set title "Normal Fibonacci Speed"
set xlabel "n^{th} Number"
set ylabel "time(nsec)"
set terminal png font " Times_New_Roman,12 "
set output "Normal_Fib.png"
set xtics 0 ,10 ,100
set key left 

plot "out" using 1 with linespoints linewidth 2 title "user space", \
"out" using 2 with linespoints linewidth 2 title "kernel space", \
"out" using 3 with linespoints linewidth 2 title "kernel to user", \