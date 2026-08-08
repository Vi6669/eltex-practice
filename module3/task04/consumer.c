#include "consumer_logic.h"

int main() {
    printf("Consumer [%d]: Starting main entry point (System V)...\n", getpid());

    // Генерация общего ключа ftok
    key_t key = ftok(FTOK_PATH, PROJ_ID);
    if (key == -1) {
        perror("ftok");
        return EXIT_FAILURE;
    }

    int shmid, semid;
    void *shm_ptr = NULL;

    // 1. Подключение к IPC ресурсам
    if (connect_consumer_ipc(key, &shmid, &semid, &shm_ptr) == -1) {
        return EXIT_FAILURE;
    }

    printf("Consumer [%d]: Connected to shared memory (shmid=%d) and semaphore (semid=%d).\n", 
           getpid(), shmid, semid);

    // 2. Запуск цикла обработки данных
    run_consumer_loop(shm_ptr, semid);

    // 3. Отключение от разделяемой памяти
    disconnect_consumer_ipc(shm_ptr);

    printf("Consumer [%d]: Detached from shared memory. Program terminated.\n", getpid());
    return EXIT_SUCCESS;
}