CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = file_copier
OBJS = main.o copier.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c copier.h
	$(CC) $(CFLAGS) -c main.c

copier.o: copier.c copier.h
	$(CC) $(CFLAGS) -c copier.c

clean:
	rm -f $(TARGET) $(OBJS) *.copy

.PHONY: all clean