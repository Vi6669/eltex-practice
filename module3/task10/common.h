#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <sys/types.h>

typedef enum {
    MSG_TASK,           // Назначение новой задачи
    MSG_STATUS_REQ,     // Запрос текущего статуса
    MSG_STATUS_RSP,     // Ответ со статусом
    MSG_ACK,            // Подтверждение приема задачи
    MSG_BUSY,           // Отказ (водитель уже занят)
    MSG_COUNT           // Общее количество типов сообщений для размера таблиц
} msg_type_t;

typedef struct {
    msg_type_t type;
    int payload;
    pid_t driver_pid;
} ipc_msg_t;

#endif // COMMON_H