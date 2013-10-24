all: wolves-squirrels-serial

wolves-squirrels-serial: wolves-squirrels-serial.c
	gcc -o wolves-squirrels-serial -g -Wall wolves-squirrels-serial.c

run: wolves-squirrels-serial
	./wolves-squirrels-serial input 4 4 4 4

gdb: wolves-squirrels-serial
	gdb --args ./wolves-squirrels-serial input 4 4 4 4

clean:
	rm -f wolves-squirrels-serial
