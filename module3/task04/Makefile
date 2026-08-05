CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -g
LIBS = -pthread -lrt

all: producer consumer

producer: producer.o
	$(CC) $(CFLAGS) -o producer producer.o $(LIBS)

consumer: consumer.o
	$(CC) $(CFLAGS) -o consumer consumer.o $(LIBS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f producer consumer *.o

.PHONY: all clean