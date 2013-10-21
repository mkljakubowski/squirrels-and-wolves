all: seq wolves-squirrels-serial

seq: seq.c
	gcc -o seq -g -Wall seq.c

wolves-squirrels-serial: wolves-squirrels-serial.c
	gcc -o wolves-squirrels-serial -g -Wall wolves-squirrels-serial.c

run: seq
	./seq input 4 4 4 4

clean:
	rm -f seq
