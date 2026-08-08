#include "producer_logic.h"

int main() {
    printf("Producer: Starting main entry point...\n");

    // Генерация ключа System V
    key_t key = ftok(FTOK_PATH, PROJ_ID);
    if (key == -1) {
        perror("ftok");
        return EXIT_FAILURE;
    }

    int shmid, semid;
    void *shm_ptr = NULL;

    // 1. Инициализация IPC ресурсов
    if (init_producer_ipc(key, &shmid, &semid, &shm_ptr) == -1) {
        return EXIT_FAILURE;
    }

    printf("Producer: Shared memory (shmid=%d) and semaphore (semid=%d) mapped successfully.\n", shmid, semid);

    // 2. Генерация блоков данных
    run_generation_loop(shm_ptr, semid);

    // 3. Ожидание завершения обработки
    wait_for_processing(shm_ptr, semid);

    // 4. Очистка ресурсов
    cleanup_producer_ipc(shmid, semid, shm_ptr);

    return EXIT_SUCCESS;
}