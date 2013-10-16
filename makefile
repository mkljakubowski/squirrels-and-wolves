all: seq

seq: seq.c
	gcc -o seq -g seq.c

clean:
	rm -f seq