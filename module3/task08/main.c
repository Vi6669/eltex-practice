#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <sys/socket.h>
#include <net/if.h>

#include "sniffer.h"
#include "filter.h"
#include "packet_processor.h"

#define BUFFER_SIZE 65536

volatile sig_atomic_t keep_running = 1;
int global_sock = -1;
char global_device[IFNAMSIZ];

// Обработчик сигнала Ctrl+C для безопасного завершения программы
void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

// Вывод интерактивного меню и чтение выбора пользователя
static int prompt_filter_choice(void) {
    printf("\nВыберите фильтр сетевого трафика:\n");
    printf("1) Групповой чат (Задание 6, структура ChatPacket)\n");
    printf("2) DNS сообщения (UDP порт 53)\n");
    printf("Ваш выбор: ");
    int choice = 0;
    if (scanf("%d", &choice) != 1 || (choice != 1 && choice != 2)) {
        return -1;
    }
    return choice;
}

// Главный бесконечный цикл перехвата и фильтрации трафика
static void run_sniffer_loop(int sock, int choice, int chat_port, FILE *log_file) {
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    unsigned char buffer[BUFFER_SIZE];
    ParsedPacket pkt;

    while (keep_running) {
        // Чтение пакета из сырого сокета
        int packet_size = recv(sock, buffer, BUFFER_SIZE, 0);
        if (packet_size < 0) {
            if (!keep_running) break; // Прерывание по Ctrl+C
            perror("Ошибка вызова recv");
            continue;
        }

        // Если пакет не удалось распарсить (не UDP, не IPv4) — пропускаем
        if (!parse_udp_packet(buffer, packet_size, &pkt)) {
            continue;
        }

        // Применяем логику фильтрации
        bool match = false;
        if (choice == 1) {
            match = filter_chat_task6(&pkt, chat_port);
        } else if (choice == 2) {
            match = filter_dns(&pkt);
        }

        // Логирование совпавшего пакета
        if (match) {
            double elapsed = get_elapsed_time(start_time);
            process_and_log_packet(&pkt, elapsed, choice, log_file);
        }
    }
}

int main(int argc, char *argv[]) {
    // Проверка аргументов запуска
    if (argc < 2) {
        fprintf(stderr, "Использование: sudo %s <сетевой_интерфейс>\n", argv[0]);
        fprintf(stderr, "Пример: sudo %s ens18\n", argv[0]);
        return EXIT_FAILURE;
    }

    strncpy(global_device, argv[1], IFNAMSIZ - 1);

    printf("=== Инициализация RAW сокета на интерфейсе: %s ===\n", global_device);
    
    // Инициализация сокета AF_PACKET
    int sock = init_raw_socket(global_device);
    if (sock < 0) {
        if (sock == -2) {
            fprintf(stderr, "Ошибка: Интерфейс %s не найден.\n", global_device);
        } else {
            fprintf(stderr, "Ошибка инициализации сокета. Проверьте права root (sudo).\n");
        }
        return EXIT_FAILURE;
    }
    global_sock = sock;

    // Попытка включения "беспорядочного" режима
    if (set_promisc_mode(sock, global_device, true) < 0) {
        fprintf(stderr, "Предупреждение: Не удалось включить беспорядочный режим.\n");
    } else {
        printf("[+] Беспорядочный режим (Promiscuous mode) успешно включен.\n");
    }

    // Выбор фильтра
    int choice = prompt_filter_choice();
    if (choice < 0) {
        fprintf(stderr, "Неверный выбор. Завершение.\n");
        set_promisc_mode(sock, global_device, false);
        close(sock);
        return EXIT_FAILURE;
    }

    // Запрос порта, если фильтруем чат
    int chat_port = 0;
    if (choice == 1) {
        printf("Введите UDP порт вашего чата (из аргументов запуска Задания 6): ");
        if (scanf("%d", &chat_port) != 1 || chat_port <= 0 || chat_port > 65535) {
            fprintf(stderr, "Неверный порт. Завершение.\n");
            set_promisc_mode(sock, global_device, false);
            close(sock);
            return EXIT_FAILURE;
        }
    }

    // Открытие файла для логирования
    FILE *log_file = fopen("capture.log", "w");
    if (!log_file) {
        fprintf(stderr, "Предупреждение: Не удалось открыть capture.log для записи.\n");
    } else {
        printf("[+] Лог сессии будет параллельно сохранен в файл: capture.log\n");
    }

    // Установка обработчика сигнала прерывания (Ctrl+C)
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    printf("\nЗапуск захвата пакетов... Нажмите Ctrl+C для остановки.\n");
    printf("========================================================\n");

    // Передаем управление в бесконечный цикл захвата
    run_sniffer_loop(sock, choice, chat_port, log_file);

    // Завершение работы: возвращаем интерфейс в обычный режим, закрываем сокет и файл
    printf("\nЗавершение сессии... Отключение беспорядочного режима.\n");
    set_promisc_mode(sock, global_device, false);
    close(sock);
    if (log_file) {
        fclose(log_file);
        printf("[+] Файл capture.log успешно сохранен.\n");
    }
    printf("Готово.\n");

    return EXIT_SUCCESS;
}