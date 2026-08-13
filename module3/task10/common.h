#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <sys/types.h>

// Типы сообщений между процессами
typedef enum {
    MSG_TASK,           // Назначение новой задачи
    MSG_STATUS_REQ,     // Запрос текущего статуса
    MSG_STATUS_RSP,     // Ответ со статусом
    MSG_ACK,            // Подтверждение приема задачи
    MSG_BUSY            // Отказ (водитель уже занят)
} msg_type_t;

// Структура IPC-сообщения
typedef struct {
    msg_type_t type;    // Тип сообщения
    int payload;        // Время (длительность или остаток секунд)
    pid_t driver_pid;   // PID отправителя (водителя)
} ipc_msg_t;

#endif // COMMON_H