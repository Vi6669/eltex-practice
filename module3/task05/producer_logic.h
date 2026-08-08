
#ifndef PRODUCER_LOGIC_H
#define PRODUCER_LOGIC_H

#include "common.h"

// Инициализация IPC разделяемой памяти и семафора POSIX
int init_producer_ipc(int *shm_fd, sem_t **sem, void **shm_ptr);

// Запуск цикла периодической генерации блоков данных
void run_generation_loop(void *shm_ptr, sem_t *sem);

// Ожидание завершения обработки всех наборов чисел потребителями
void wait_for_processing(void *shm_ptr, sem_t *sem);

// Удаление ресурсов POSIX IPC
void cleanup_producer_ipc(int shm_fd, sem_t *sem, void *shm_ptr);

#endif /* PRODUCER_LOGIC_H */
