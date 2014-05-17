CC = gcc
LD = gcc
CFLAGS = -Wall -g -O0 -std=gnu99
LDFLAGS =

all: ish

ish: parser ish.o
	$(CC) $(CFLAGS) ish.o parser/parse.o parser/print.o -o ish

ish.o: config.h

parser:
	cd parser; make

clean:
	cd parser; make clean
	rm -f *.o myexec ish

.PHONY: all clean parser
