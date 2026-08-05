#include "common.h"

int main() {
    printf("Producer: Initialization...\n");

    // Удаляем старые объекты на случай, если предыдущий запуск завершился аварийно
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);

    // Создаем разделяемую память
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return EXIT_FAILURE;
    }

    // Устанавливаем размер сегмента
    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate");
        close(shm_fd);
        return EXIT_FAILURE;
    }

    // Проецируем память в адресное пространство процесса
    void *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return EXIT_FAILURE;
    }

    // Создаем семафор с начальным значением 1 (свободен)
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        munmap(shm_ptr, SHM_SIZE);
        close(shm_fd);
        return EXIT_FAILURE;
    }

    // Записываем начальные значения в заголовок под защитой семафора
    sem_wait(sem);
    struct ShmHeader *header = (struct ShmHeader *)shm_ptr;
    header->first_block_offset = 0;
    header->free_offset = sizeof(struct ShmHeader);
    header->producer_done = 0;
    sem_post(sem);

    printf("Producer: Shared memory and semaphore initialized.\n");

    // Временное освобождение ресурсов для Этапа 1
    sem_close(sem);
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);

    return EXIT_SUCCESS;
}