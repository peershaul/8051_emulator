CC = gcc


all: bulid/main.o bulid/emulator.o
	${CC} -g bulid/main.o bulid/emulator.o -o bulid/emulator

bulid/main.o: main.c
	${CC} -g -c main.c -o bulid/main.o

bulid/emulator.o: emulator.c
	${CC} -g -c emulator.c -o bulid/emulator.o
