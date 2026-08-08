
#include "producer_logic.h"

int main() {
    printf("Producer: Starting main entry point (POSIX)...\n");

    int shm_fd;
    sem_t *sem = NULL;
    void *shm_ptr = NULL;

    // 1. Инициализация ресурсов POSIX
    if (init_producer_ipc(&shm_fd, &sem, &shm_ptr) == -1) {
        return EXIT_FAILURE;
    }

    printf("Producer: Shared memory (fd=%d) and semaphore mapped successfully.\n", shm_fd);

    // 2. Генерация блоков данных
    run_generation_loop(shm_ptr, sem);

    // 3. Ожидание завершения обработки
    wait_for_processing(shm_ptr, sem);

    // 4. Очистка ресурсов
    cleanup_producer_ipc(shm_fd, sem, shm_ptr);

    return EXIT_SUCCESS;
}
