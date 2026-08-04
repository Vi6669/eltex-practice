#include "broker.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <errno.h>
#include <unistd.h>

// Вспомогательная функция создания эксклюзивной очереди
static int create_exclusive_queue(void) {
    key_t key = get_ipc_key();
    int msqid = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    if (msqid == -1) {
        if (errno == EEXIST) {
            fprintf(stderr, "[Брокер] Ошибка: Очередь сообщений уже создана другим брокером.\n");
            exit(EXIT_FAILURE);
        } else {
            perror("msgget (ошибка создания очереди)");
            exit(EXIT_FAILURE);
        }
    }
    return msqid;
}

void run_broker(void) {
    printf("[Брокер] Запуск...\n");
    
    int msqid = create_exclusive_queue();
    printf("[Брокер] Очередь сообщений успешно создана (ID: %d).\n", msqid);

    // Будущий цикл обработки входящих подписок и публикаций
    while (1) {
        sleep(1);
    }
}