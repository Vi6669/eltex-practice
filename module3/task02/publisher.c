#include "publisher.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

void run_publisher(const char *topic) {
    printf("[Издатель] Запуск для темы: '%s'...\n", topic);
    
    int msqid = connect_to_queue();
    printf("[Издатель] Успешно подключен к очереди (ID: %d). Мой PID: %d\n", msqid, getpid());
    printf("[Издатель] Введите текст сообщения (Ctrl+D для выхода):\n");

    char line[256];
    struct msgbuf pub_msg;
    pub_msg.mtype = 1; // Все системные запросы брокеру шлются на mtype = 1

    // Интерактивный режим ввода строк
    while (fgets(line, sizeof(line), stdin)) {
        // Убираем символ перевода строки
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;

        // Формируем сообщение: "send,PID,TOPIC,TEXT"
        snprintf(pub_msg.mtext, sizeof(pub_msg.mtext), "send,%d,%s,%s", getpid(), topic, line);
        
        if (msgsnd(msqid, &pub_msg, strlen(pub_msg.mtext) + 1, 0) == -1) {
            perror("[Издатель] Ошибка отправки сообщения");
            break;
        }
        printf("[Издатель] Сообщение отправлено брокеру.\n");
    }

    printf("[Издатель] Завершение работы.\n");
}