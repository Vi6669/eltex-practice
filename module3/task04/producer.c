#include "common.h"

int main() {
    printf("Producer: Starting initialization...\n");

    // Очистка старых объектов IPC
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);

    // Инициализация памяти
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return EXIT_FAILURE;
    }

    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate");
        close(shm_fd);
        return EXIT_FAILURE;
    }

    void *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return EXIT_FAILURE;
    }

    // Инициализация семафора
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        munmap(shm_ptr, SHM_SIZE);
        close(shm_fd);
        return EXIT_FAILURE;
    }

    // Настройка начального состояния заголовка
    sem_wait(sem);
    struct ShmHeader *header = (struct ShmHeader *)shm_ptr;
    header->first_block_offset = 0;
    header->free_offset = sizeof(struct ShmHeader);
    header->producer_done = 0;
    sem_post(sem);

    printf("Producer: Shared memory and semaphore initialized.\n");

    // Инициализируем генератор случайных чисел
    srand(getpid() ^ time(NULL));

    size_t last_block_offset = 0;

    // Цикл генерации данных
    while (1) {
        usleep(500000); // Имитация периодической генерации (0.5 сек)

        sem_wait(sem);

        // Случайный размер массива от 5 до 15 элементов
        size_t n = 5 + (rand() % 11);
        size_t block_size = sizeof(struct Block) + n * sizeof(int);

        // Проверяем, помещается ли блок в оставшуюся разделяемую память
        if (header->free_offset + block_size > SHM_SIZE) {
            printf("\nProducer: Memory limits reached. Stopping data generation.\n");
            header->producer_done = 1;
            sem_post(sem);
            break;
        }

        // Вычисляем смещение для нового блока
        size_t new_block_offset = header->free_offset;
        struct Block *new_block = (struct Block *)((char *)shm_ptr + new_block_offset);

        // Заполняем структуру блока
        new_block->num_elements = n;
        new_block->next_block_offset = 0;

        printf("Producer generated block at offset %zu with %zu elements: [ ", new_block_offset, n);
        for (size_t i = 0; i < n; i++) {
            new_block->data[i] = rand() % 100; // Случайное число от 0 до 99
            printf("%d ", new_block->data[i]);
        }
        printf("]\n");

        // Связываем блоки в односвязный список через смещения
        if (header->first_block_offset == 0) {
            header->first_block_offset = new_block_offset;
        } else {
            struct Block *prev_block = (struct Block *)((char *)shm_ptr + last_block_offset);
            prev_block->next_block_offset = new_block_offset;
        }

        // Обновляем состояние в разделяемой памяти
        last_block_offset = new_block_offset;
        header->free_offset += block_size;

        sem_post(sem);
    }

    // Цикл ожидания обработки данных потребителями
    printf("Producer: Waiting for consumers to finish processing all blocks...\n");
    while (1) {
        sleep(1); // Периодическая проверка (1 сек)

        sem_wait(sem);

        int all_processed = 1;
        size_t current_offset = header->first_block_offset;

        // Проверяем по цепочке смещений, есть ли необработанные блоки
        while (current_offset != 0) {
            struct Block *block = (struct Block *)((char *)shm_ptr + current_offset);
            if (block->num_elements > 0) {
                all_processed = 0;
                break;
            }
            current_offset = block->next_block_offset;
        }

        if (all_processed) {
            printf("Producer: All blocks have been successfully processed!\n");
            sem_post(sem);
            break;
        }

        sem_post(sem);
    }

    // Финальная очистка ресурсов
    sem_close(sem);
    sem_unlink(SEM_NAME);
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
    shm_unlink(SHM_NAME);

    printf("Producer: Cleanup done. Exiting.\n");
    return EXIT_SUCCESS;
}