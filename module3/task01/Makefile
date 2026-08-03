CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = file_copier
TEST_TARGET = tester
OBJS = main.o copier.o
TEST_OBJS = tester.o copier.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c copier.h
	$(CC) $(CFLAGS) -c main.c

copier.o: copier.c copier.h
	$(CC) $(CFLAGS) -c copier.c

# Компиляция объектного файла тестировщика
tester.o: tester.c tester.h copier.h
	$(CC) $(CFLAGS) -c tester.c

# Сборка и непосредственный запуск тестировщика
tester: $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $(TEST_TARGET) $(TEST_OBJS)
	./$(TEST_TARGET)

# Очистка всех бинарников, объектных файлов и тестовых копий
clean:
	rm -f $(TARGET) $(TEST_TARGET) $(OBJS) $(TEST_OBJS) *.copy test_*.txt test_fifo*

.PHONY: all clean tester