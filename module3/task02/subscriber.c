#include "subscriber.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

static volatile sig_atomic_t shutdown_flag = 0;

static void subscriber_signal_handler(int sig) {
    if (sig == SIGINT) {
        shutdown_flag = 1;
    }
}

void run_subscriber(int topic_count, char **topics) {
    printf("[Подписчик] Запуск...\n");

    int msqid = connect_to_queue();
    printf("[Подписчик] Подключен к очереди (ID: %d). Мой PID: %d\n", msqid, getpid());

    // Настройка обработки SIGINT
    struct sigaction sa;
    sa.sa_handler = subscriber_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Не устанавливаем SA_RESTART для прерывания msgrcv
    sigaction(SIGINT, &sa, NULL);

    struct msgbuf sub_msg;
    sub_msg.mtype = 1;

    // Подписываемся на каждую указанную тему
    for (int i = 0; i < topic_count; i++) {
        snprintf(sub_msg.mtext, sizeof(sub_msg.mtext), "subscribe,%d,%s", getpid(), topics[i]);
        
        if (msgsnd(msqid, &sub_msg, strlen(sub_msg.mtext) + 1, 0) == -1) {
            if (errno == EIDRM || errno == EINVAL) {
                printf("[Подписчик] Очередь сообщений была удалена. Выход...\n");
                exit(EXIT_FAILURE);
            }
            perror("[Подписчик] Ошибка отправки подписки");
            exit(EXIT_FAILURE);
        }
        printf("[Подписчик] Отправлена подписка на тему: '%s'\n", topics[i]);
    }

    printf("[Подписчик] Ожидание сообщений от брокера...\n");

    struct msgbuf rx_msg;
    while (!shutdown_flag) {
        if (msgrcv(msqid, &rx_msg, sizeof(rx_msg.mtext), getpid(), 0) == -1) {
            if (errno == EINTR || shutdown_flag) {
                break; // Выход по сигналу
            }
            if (errno == EIDRM || errno == EINVAL) {
                printf("\n[Подписчик] Очередь сообщений была удалена брокером. Выход...\n");
                break;
            }
            perror("[Подписчик] Ошибка получения");
            break;
        }
        printf("[Подписчик] Получено: %s\n", rx_msg.mtext);
    }

    // Если выход осуществлен по сигналу, отправляем брокеру запросы на отписку
    if (shutdown_flag) {
        printf("\n[Подписчик] Завершение работы по сигналу SIGINT. Отписка от тем...\n");
        for (int i = 0; i < topic_count; i++) {
            struct msgbuf unsub_msg;
            unsub_msg.mtype = 1;
            snprintf(unsub_msg.mtext, sizeof(unsub_msg.mtext), "unsubscribe,%d,%s", getpid(), topics[i]);
            
            // Используем IPC_NOWAIT, чтобы отписка не зависла, если очередь заблокирована
            if (msgsnd(msqid, &unsub_msg, strlen(unsub_msg.mtext) + 1, IPC_NOWAIT) == -1) {
                if (errno == EIDRM || errno == EINVAL) {
                    // Очередь уже удалена брокером, отписка не требуется
                    break;
                }
            }
        }
    }

    printf("[Подписчик] Завершение работы.\n");
}