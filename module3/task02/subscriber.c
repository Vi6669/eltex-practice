#include "subscriber.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

void run_subscriber(int topic_count, char **topics) {
    printf("[Подписчик] Запуск...\n");

    int msqid = connect_to_queue();
    printf("[Подписчик] Успешно подключен к очереди (ID: %d). Мой PID: %d\n", msqid, getpid());

    struct msgbuf sub_msg;
    sub_msg.mtype = 1; // Все запросы брокеру шлются на mtype = 1

    // Отправляем запросы на подписку для каждой переданной темы
    for (int i = 0; i < topic_count; i++) {
        snprintf(sub_msg.mtext, sizeof(sub_msg.mtext), "subscribe,%d,%s", getpid(), topics[i]);
        
        if (msgsnd(msqid, &sub_msg, strlen(sub_msg.mtext) + 1, 0) == -1) {
            perror("[Подписчик] Ошибка отправки подписки");
            exit(EXIT_FAILURE);
        }
        printf("[Подписчик] Отправлен запрос на подписку: '%s'\n", topics[i]);
    }

    printf("[Подписчик] Ожидание сообщений от брокера...\n");

    struct msgbuf rx_msg;
    while (1) {
        // Подписчик слушает только сообщения с типом, равным его PID
        if (msgrcv(msqid, &rx_msg, sizeof(rx_msg.mtext), getpid(), 0) == -1) {
            perror("[Подписчик] Ошибка получения");
            break;
        }
        printf("[Подписчик] Получено: %s\n", rx_msg.mtext);
    }
}