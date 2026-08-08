
#ifndef CONSUMER_LOGIC_H
#define CONSUMER_LOGIC_H

#include "common.h"

// Подключение к существующим ресурсам POSIX IPC
int connect_consumer_ipc(int *shm_fd, sem_t **sem, void **shm_ptr);

// Цикл обработки данных
void run_consumer_loop(void *shm_ptr, sem_t *sem);

// Отключение разделяемой памяти и закрытие семафора
void disconnect_consumer_ipc(int shm_fd, void *shm_ptr, sem_t *sem);

#endif /* CONSUMER_LOGIC_H */
