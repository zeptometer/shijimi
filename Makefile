CC = gcc
LD = gcc
CFLAGS = -Wall
LDFLAGS =

all: myexec ish

ish: parser ish.o
	$(CC) $(CFLAGS) ish.o parser/parse.o -o ish

parser:
	cd parser; make

myexec: myexec.o
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	cd parser; make clean
	rm -f *.o myexec ish

.PHONY: all clean parser
