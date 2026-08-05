#include "common.h"

int main() {
    printf("Consumer: Connecting...\n");

    // Подключаемся к существующей разделяемой памяти
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open (is producer running?)");
        return EXIT_FAILURE;
    }

    // Проецируем память
    void *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return EXIT_FAILURE;
    }

    // Открываем существующий семафор
    sem_t *sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        munmap(shm_ptr, SHM_SIZE);
        close(shm_fd);
        return EXIT_FAILURE;
    }

    // Читаем заголовок под семафором
    sem_wait(sem);
    struct ShmHeader *header = (struct ShmHeader *)shm_ptr;
    printf("Consumer connected. Header info: free_offset = %zu, producer_done = %d\n",
           header->free_offset, header->producer_done);
    sem_post(sem);

    // Закрываем дескрипторы
    sem_close(sem);
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);

    return EXIT_SUCCESS;
}