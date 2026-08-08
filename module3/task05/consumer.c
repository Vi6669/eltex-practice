
#include "consumer_logic.h"

int main() {
    printf("Consumer [%d]: Starting main entry point (POSIX)...\n", getpid());

    int shm_fd;
    sem_t *sem = NULL;
    void *shm_ptr = NULL;

    // 1. Подключение к IPC ресурсам
    if (connect_consumer_ipc(&shm_fd, &sem, &shm_ptr) == -1) {
        return EXIT_FAILURE;
    }

    printf("Consumer [%d]: Connected to shared memory and semaphore.\n", getpid());

    // 2. Запуск цикла обработки данных
    run_consumer_loop(shm_ptr, sem);

    // 3. Отключение от разделяемой памяти
    disconnect_consumer_ipc(shm_fd, shm_ptr, sem);

    printf("Consumer [%d]: Detached. Program terminated.\n", getpid());
    return EXIT_SUCCESS;
}
