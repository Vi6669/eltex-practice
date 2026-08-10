CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -g
LIBS = -pthread -lrt

# По умолчанию собираем все программы (тесты только компилируются)
all: producer consumer tester

# Сборка производителя
producer: producer.o producer_logic.o
	$(CC) $(CFLAGS) -o producer producer.o producer_logic.o $(LIBS)

# Сборка потребителя
consumer: consumer.o consumer_logic.o
	$(CC) $(CFLAGS) -o consumer consumer.o consumer_logic.o $(LIBS)

# Сборка тестов (только компиляция бинарника)
tester: tester.o
	$(CC) $(CFLAGS) -o tester tester.o $(LIBS)

# Сборка и принудительный запуск тестов (make test)
test: tester
	./tester

# Правила компиляции объектных файлов
producer.o: producer.c producer_logic.h common.h
	$(CC) $(CFLAGS) -c producer.c

producer_logic.o: producer_logic.c producer_logic.h common.h
	$(CC) $(CFLAGS) -c producer_logic.c

consumer.o: consumer.c consumer_logic.h common.h
	$(CC) $(CFLAGS) -c consumer.c

consumer_logic.o: consumer_logic.c consumer_logic.h common.h
	$(CC) $(CFLAGS) -c consumer_logic.c

tester.o: tester.c tester.h common.h
	$(CC) $(CFLAGS) -c tester.c

# Очистка проекта
clean:
	rm -f producer consumer tester *.o


.PHONY: all clean test