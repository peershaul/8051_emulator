# Default variables, added by Maker
LIBS = 
CFLAGS = -g

CC = gcc


#begin_unique

#end_unique

# Maker added code!
bulid/objects/CPU.o: /smallmass/dev/8051_emulator/CPU.c
	${CC} ${CFLAGS} -c /smallmass/dev/8051_emulator/CPU.c -o bulid/objects/CPU.o ${LIBS}

bulid/objects/main.o: /smallmass/dev/8051_emulator/main.c
	${CC} ${CFLAGS} -c /smallmass/dev/8051_emulator/main.c -o bulid/objects/main.o ${LIBS}

all: bulid/objects/CPU.o bulid/objects/main.o 
	${CC} ${CFLAGS} -o bulid/maker.out ${LIBS} bulid/objects/CPU.o bulid/objects/main.o 
clean:
	rm bulid/objects/*
	rm bulid/maker.out
