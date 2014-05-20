CC       = gcc
CFLAGS   = -std=gnu99 -Wall -Wextra 
LDFLAGS  =
TARGET   = ./bin/shijimi
OBJECTS  = main.o procset.o parse.o

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

all: clean $(TARGET)

clean: .PHONY
	rm -f $(OBJECTS) $(TARGET)

.PHONY:
