set title "Compare Fibonacci 1000 Kernel Speed"
set xlabel "n^{th} Number"
set ylabel "time(nsec)"
set terminal png font " Times_New_Roman,12 "
set output "Compare_First_1000_Fib_Kernel_Time.png"
set xtics 0 ,100 ,1000
set grid
set key left 

plot "plot" using 1 with linespoints linewidth 2 title "long db clz (v0)", \
"plot" using 2 with linespoints linewidth 2 title "long db clz (v1)", \