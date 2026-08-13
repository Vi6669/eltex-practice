CC = gcc
CFLAGS = -Wall -Wextra -g

# Поиск всех исходных файлов .c в текущей папке
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
BIN = taxi_app

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(BIN)
	rm -f /tmp/taxi_req_* /tmp/taxi_rsp_*