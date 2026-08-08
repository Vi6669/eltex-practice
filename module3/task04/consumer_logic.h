#ifndef CONSUMER_LOGIC_H
#define CONSUMER_LOGIC_H

#include "common.h"

// Подключение к существующим IPC ресурсам (разделяемая память и семафор)
// Возвращает 0 при успехе, -1 при ошибке
int connect_consumer_ipc(key_t key, int *shmid, int *semid, void **shm_ptr);

// Запуск цикла обработки данных потребителем
void run_consumer_loop(void *shm_ptr, int semid);

// Отключение от разделяемой памяти
void disconnect_consumer_ipc(void *shm_ptr);

#endif /* CONSUMER_LOGIC_H */