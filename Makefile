CC = gcc
CFLAGS = -Wall -g -O0
SRCS = parser.c

all: parse.o print.o

clean:
	$(RM) *.o

.PHONY: all clean
