#include "publisher.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

static volatile sig_atomic_t shutdown_flag = 0;

static void publisher_signal_handler(int sig) {
    if (sig == SIGINT) {
        shutdown_flag = 1;
    }
}

void run_publisher(const char *topic) {
    printf("[Издатель] Запуск для темы: '%s'...\n", topic);
    
    int msqid = connect_to_queue();
    printf("[Издатель] Подключен к очереди (ID: %d). Мой PID: %d\n", msqid, getpid());

    // Настройка перехвата SIGINT
    struct sigaction sa;
    sa.sa_handler = publisher_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Не устанавливаем SA_RESTART, чтобы прервать fgets при SIGINT
    sigaction(SIGINT, &sa, NULL);

    printf("[Издатель] Введите текст сообщения (Ctrl+D для выхода):\n");

    char line[256];
    struct msgbuf pub_msg;
    pub_msg.mtype = 1;

    while (!shutdown_flag) {
        printf("> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            if (shutdown_flag) {
                break; // Выход по сигналу SIGINT
            }
            if (feof(stdin)) {
                break; // Выход по Ctrl+D
            }
            if (errno == EINTR) {
                continue; // Прерывание, но не SIGINT — повторяем попытку
            }
            break;
        }

        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;

        snprintf(pub_msg.mtext, sizeof(pub_msg.mtext), "send,%d,%s,%s", getpid(), topic, line);
        
        if (msgsnd(msqid, &pub_msg, strlen(pub_msg.mtext) + 1, 0) == -1) {
            if (errno == EIDRM || errno == EINVAL) {
                printf("\n[Издатель] Очередь сообщений была удалена брокером. Выход...\n");
            } else {
                perror("[Издатель] Ошибка отправки");
            }
            break;
        }
        printf("[Издатель] Отправлено.\n");
    }

    printf("[Издатель] Завершение работы.\n");
}