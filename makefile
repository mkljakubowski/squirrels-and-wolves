all: wolves-squirrels-serial wolves-squirrels-omp

wolves-squirrels-serial: wolves-squirrels-serial.c
	gcc -o wolves-squirrels-serial -g -Wall wolves-squirrels-serial.c

wolves-squirrels-omp: wolves-squirrels-omp.c
	gcc -o wolves-squirrels-omp -g -Wall -fopenmp wolves-squirrels-omp.c

run: wolves-squirrels-serial
	./wolves-squirrels-serial input 4 4 4 4

run2: wolves-squirrels-serial
	./wolves-squirrels-serial ex3.in 3 4 4 4

gdb: wolves-squirrels-serial
	gdb --args ./wolves-squirrels-serial input 4 4 4 4

clean:
	rm -f wolves-squirrels-serial wolves-squirrels-omp
