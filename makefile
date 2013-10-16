all: seq

seq: seq.c
	gcc -o seq -g -Wall seq.c

run: seq
	./seq input 4 4 4 4
	
clean:
	rm -f seq