all: wolves-squirrels-serial wolves-squirrels-omp

wolves-squirrels-serial: wolves-squirrels-serial.c
	gcc -o wolves-squirrels-serial -g -Wall wolves-squirrels-serial.c

wolves-squirrels-omp: wolves-squirrels-omp.c
	gcc -o wolves-squirrels-omp -g -Wall -fopenmp wolves-squirrels-omp.c

run-serial: wolves-squirrels-serial
	./wolves-squirrels-serial input 4 4 4 4

run-serial2: wolves-squirrels-serial
	./wolves-squirrels-serial ex3.in 10 10 10 1000 > serial

run-omp: wolves-squirrels-omp
	./wolves-squirrels-omp ex3.in 10 10 10 2000 > parallel

gdb: wolves-squirrels-serial
	gdb --args ./wolves-squirrels-serial ex3.in 10 10 10 10

time: all
	time ./wolves-squirrels-serial exBig.in 4 4 4 4
	time ./wolves-squirrels-omp exBig.in 4 4 4 4

1:
	export OMP_NUM_THREADS=1
	
2:
	export OMP_NUM_THREADS=2
	
4:
	export OMP_NUM_THREADS=4
	
8:
	export OMP_NUM_THREADS=8

clean:
	rm -f wolves-squirrels-serial wolves-squirrels-omp
