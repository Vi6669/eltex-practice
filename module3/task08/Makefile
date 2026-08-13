CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -O2
TARGET = sniffer

SRCS = main.c sniffer.c filter.c packet_processor.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) capture.log

.PHONY: all clean