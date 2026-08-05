#ifndef CHAT_H
#define CHAT_H

#include <mqueue.h>
#include <pthread.h>

#define MSG_BUFFER_SIZE 256
#define MAX_MESSAGES 10
#define EXIT_PRIORITY 255 // Приоритет для сообщения о выходе

// Структура для хранения состояния текущей сессии чата
typedef struct {
    char q1_name[128];
    char q2_name[128];
    mqd_t rx_q;
    mqd_t tx_q;
    int is_creator;
    pthread_t rx_thread;
    int peer_exited; // Флаг завершил ли собеседник работу первым
} chat_session_t;

// Форматирование имен очередей
void format_queue_names(const char *base_name, chat_session_t *session);

// Инициализация и открытие очередей
int init_chat_queues(chat_session_t *session);

// Основной цикл чата (потоки чтения и отправки)
void run_chat_loop(chat_session_t *session);

// Функция фонового потока для чтения из очереди
void *receive_thread_func(void *arg);

// Закрытие и удаление очередей
void cleanup_chat_queues(chat_session_t *session);

#endif // CHAT_H