CC = gcc
CFLAGS = -Wall -Wextra -O2

all: server client

# Компиляция модуля утилит (контрольные суммы, сборка UDP)
utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c utils.c

# Компиляция модуля обработки сообщений и БД сервера (САОД)
server_logic.o: server_logic.c server_logic.h utils.h
	$(CC) $(CFLAGS) -c server_logic.c

# Компиляция модуля отправки/приема и биндинга клиента
client_logic.o: client_logic.c client_logic.h utils.h
	$(CC) $(CFLAGS) -c client_logic.c

# Сборка исполняемого файла сервера
server: server_main.c server_logic.o utils.o
	$(CC) $(CFLAGS) server_main.c server_logic.o utils.o -o server

# Сборка исполняемого файла клиента
client: client_main.c client_logic.o utils.o
	$(CC) $(CFLAGS) client_main.c client_logic.o utils.o -o client

# Очистка рабочей директории от временных файлов сборки
clean:
	rm -f *.o server client