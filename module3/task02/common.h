#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

#define QUEUE_PROJECT_ID 'B'
#define QUEUE_PATH "/tmp"
#define MAX_TEXT_LEN 512

// Базовая структура сообщения System V
struct msgbuf {
    long mtype;
    char mtext[MAX_TEXT_LEN];
};

// Получение ключа IPC через ftok
key_t get_ipc_key(void);

// Подключение к уже существующей очереди (для издателей и подписчиков)
int connect_to_queue(void);

#endif // COMMON_H