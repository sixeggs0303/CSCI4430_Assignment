CC = gcc
LIB = ../myftp.c -lm -lisal -lpthread

all: files

files: files.c
	${CC} -o files files.c ${LIB}

clean:
	rm files