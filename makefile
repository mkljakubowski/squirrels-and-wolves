GENS=999

# parallel make
export NUMCPUS:=$(shell grep -c ^processor /proc/cpuinfo)

all: wolves-squirrels-serial wolves-squirrels-omp wolves-squirrels-mpi

wolves-squirrels-serial: wolves-squirrels-serial.c
	gcc -o wolves-squirrels-serial -g -Wall wolves-squirrels-serial.c

wolves-squirrels-omp: wolves-squirrels-omp.c
	gcc -o wolves-squirrels-omp -g -Wall -fopenmp wolves-squirrels-omp.c

run-serial: wolves-squirrels-serial
	./wolves-squirrels-serial ex3.in 10 10 10 $(GENS)

run-serial2: wolves-squirrels-serial
	./wolves-squirrels-serial ex3.in 10 10 10 1000 > serial

valgrind-serial: wolves-squirrels-serial
	valgrind ./wolves-squirrels-serial ex3.in 10 10 10 10 --leak-check=full --track-origins=yes

gdb-serial: wolves-squirrels-serial
	gdb --args ./wolves-squirrels-serial ex3.in 10 10 10 10

run-omp: wolves-squirrels-omp
	./wolves-squirrels-omp ex3.in 10 10 10 2000 > parallel

1:
	export OMP_NUM_THREADS=1

2:
	export OMP_NUM_THREADS=2

4:
	export OMP_NUM_THREADS=4

8:
	export OMP_NUM_THREADS=8

valgrind-omp: wolves-squirrels-omp
	valgrind ./wolves-squirrels-omp ex3.in 10 10 10 10 --leak-check=full --track-origins=yes

wolves-squirrels-mpi: wolves-squirrels-mpi.c
	mpicc -o wolves-squirrels-mpi wolves-squirrels-mpi.c -g -Wall

wolves-squirrels-mpi-tests: wolves-squirrels-mpi-tests.c
	mpicc -o wolves-squirrels-mpi-tests wolves-squirrels-mpi-tests.c -g -Wall -pedantic -ansi

MPI_Isend_MPI_Irecv: MPI_Isend_MPI_Irecv.c
	mpicc -o MPI_Isend_MPI_Irecv MPI_Isend_MPI_Irecv.c -g -Wall -pedantic -ansi

run-mpi: wolves-squirrels-mpi
	mpirun -np $(NUMCPUS) ./wolves-squirrels-mpi ex3.in 10 10 10 $(GENS)
	
run-mpi-tests: wolves-squirrels-mpi-tests
	mpirun -np $(NUMCPUS) wolves-squirrels-mpi-tests ex3.in 10 10 10 10

run-MPI_Isend_MPI_Irecv: MPI_Isend_MPI_Irecv
	mpirun -np $(NUMCPUS) MPI_Isend_MPI_Irecv 1024

gdb-mpi: wolves-squirrels-mpi
	mpirun -np $(NUMCPUS) xterm -e gdb --args ./wolves-squirrels-mpi ex3.in 10 10 10 10

gdb-mpi-tests: wolves-squirrels-mpi-tests
	mpirun -np $(NUMCPUS) xterm -e gdb --args ./wolves-squirrels-mpi-tests ex3.in 10 10 10 10

valgrind-mpi: wolves-squirrels-mpi
	valgrind ./wolves-squirrels-mpi ex3.in 10 10 10 10 --leak-check=full --track-origins=yes

wolves-squirrels-mpi+omp: wolves-squirrels-mpi+omp.c
	mpicc -fopenmp wolves-squirrels-mpi+omp.c -o wolves-squirrels-mpi+omp -g -Wall

run-mpi+omp:
	mpirun -np $(NUMCPUS) wolves-squirrels-mpi+omp ex3.in 10 10 10 10

time: all
	time ./wolves-squirrels-serial exBig.in 4 4 4 4
	time ./wolves-squirrels-omp exBig.in 4 4 4 4
	time ./wolves-squirrels-mpi exBig.in 4 4 4 4
	time ./wolves-squirrels-mpi+omp exBig.in 4 4 4 4

clean:
	rm -f wolves-squirrels-serial wolves-squirrels-omp wolves-squirrels-mpi wolves-squirrels-mpi+omp wolves-squirrels-mpi-tests MPI_Isend_MPI_Irecv
