CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
LDFLAGS = -lrt -pthread

TARGET = task03
TEST_TARGET = test_runner
SRCS = main.c chat.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


test: test_chat.o chat.o
	$(CC) $(CFLAGS) -o $(TEST_TARGET) test_chat.o chat.o $(LDFLAGS)
	./$(TEST_TARGET)
	rm -f $(TEST_TARGET) test_chat.o

clean:
	rm -f $(TARGET) $(TEST_TARGET) $(OBJS) test_chat.o

.PHONY: all clean test