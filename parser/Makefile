CC = gcc
CFLAGS = -Wall
SRCS = parser.c

all: parse.o

parser.o:
	$(CC) $(CLAGS) parse.c print.c -c -o parse.o

clean:
	$(RM) *.o

.PHONY: all clean
