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

# Цель для запуска C-тестов
test: common.o broker.o unit_tests.o
	$(CC) $(CFLAGS) common.o broker.o unit_tests.o -o test_suite
	./test_suite
	rm -f test_suite unit_tests.o

clean:
	rm -f $(OBJS) $(TARGET) test_suite unit_tests.o