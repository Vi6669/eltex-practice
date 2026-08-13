CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -D_GNU_SOURCE

all: server client

# Линковка исполняемых файлов
server: server.o common.o
	$(CC) $(CFLAGS) -o server server.o common.o

client: client.o common.o
	$(CC) $(CFLAGS) -o client client.o common.o

# Компиляция объектных файлов
common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c -o common.o

server.o: server.c server.h common.h
	$(CC) $(CFLAGS) -c server.c -o server.o

client.o: client.c client.h common.h
	$(CC) $(CFLAGS) -c client.c -o client.o

clean:
	rm -f *.o server client received_*