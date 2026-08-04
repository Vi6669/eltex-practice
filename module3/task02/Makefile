CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99
TARGET = task02

SRCS = main.c common.c broker.c publisher.c subscriber.c
OBJS = $(SRCS:.c=.o)
HEADERS = common.h broker.h publisher.h subscriber.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)