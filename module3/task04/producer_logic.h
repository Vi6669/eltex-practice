#ifndef PRODUCER_LOGIC_H
#define PRODUCER_LOGIC_H

#include "common.h"

// Инициализация IPC разделяемой памяти и семафора, возврат дескрипторов и указателя
int init_producer_ipc(key_t key, int *shmid, int *semid, void **shm_ptr);

// Запуск бесконечного цикла периодической генерации блоков данных
void run_generation_loop(void *shm_ptr, int semid);

// Ожидание завершения обработки всех наборов чисел потребителями
void wait_for_processing(void *shm_ptr, int semid);

// Освобождение и удаление системных ресурсов System V IPC
void cleanup_producer_ipc(int shmid, int semid, void *shm_ptr);

#endif /* PRODUCER_LOGIC_H */