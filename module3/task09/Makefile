CC = gcc
CFLAGS = -Wall -Wextra -O2

all: server client


utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c utils.c

server_logic.o: server_logic.c server_logic.h utils.h
	$(CC) $(CFLAGS) -c server_logic.c

client_logic.o: client_logic.c client_logic.h utils.h
	$(CC) $(CFLAGS) -c client_logic.c

server: server_main.c server_logic.o utils.o
	$(CC) $(CFLAGS) server_main.c server_logic.o utils.o -o server

client: client_main.c client_logic.o utils.o
	$(CC) $(CFLAGS) client_main.c client_logic.o utils.o -o client

clean:
	rm -f *.o server client