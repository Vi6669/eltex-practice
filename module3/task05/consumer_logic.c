
#include "consumer_logic.h"

int connect_consumer_ipc(int *shm_fd, sem_t **sem, void **shm_ptr) {
    // Подключаемся к разделяемой памяти (shm_open)
    *shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (*shm_fd == -1) {
        perror("shm_open (is producer running?)");
        return -1;
    }

    // Отображаем разделяемую память (mmap)
    *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, *shm_fd, 0);
    if (*shm_ptr == MAP_FAILED) {
        perror("mmap");
        close(*shm_fd);
        return -1;
    }

    // Подключаемся к семафору (sem_open)
    *sem = sem_open(SEM_NAME, 0);
    if (*sem == SEM_FAILED) {
        perror("sem_open");
        munmap(*shm_ptr, SHM_SIZE);
        close(*shm_fd);
        return -1;
    }

    return 0;
}

void run_consumer_loop(void *shm_ptr, sem_t *sem) {
    while (1) {
        sem_wait(sem);

        struct ShmHeader *header = (struct ShmHeader *)shm_ptr;
        size_t current_offset = header->first_block_offset;
        struct Block *block_to_process = NULL;
        size_t block_offset = 0;

        // Поиск первого необработанного блока по цепочке смещений списка
        while (current_offset != 0) {
            struct Block *block = (struct Block *)((char *)shm_ptr + current_offset);
            if (block->num_elements > 0) {
                block_to_process = block;
                block_offset = current_offset;
                break;
            }
            current_offset = block->next_block_offset;
        }

        if (block_to_process != NULL) {
            size_t n = block_to_process->num_elements;
            int min = block_to_process->data[0];
            int max = block_to_process->data[0];

            printf("Consumer [%d]: Processing block at offset %zu with %zu elements: [ ", 
                   getpid(), block_offset, n);
            for (size_t i = 0; i < n; i++) {
                int val = block_to_process->data[i];
                printf("%d ", val);
                if (val < min) min = val;
                if (val > max) max = val;
            }
            printf("] -> Min: %d, Max: %d\n", min, max);

            // Помечаем блок как обработанный
            block_to_process->num_elements = 0;

            sem_post(sem);

            // Спим некоторое время после обработки
            sleep(2);
        } else {
            if (header->producer_done) {
                printf("Consumer [%d]: All blocks processed and Producer is done. Exiting loop.\n", getpid());
                sem_post(sem);
                break;
            } else {
                sem_post(sem);
                printf("Consumer [%d]: No unprocessed blocks. Waiting...\n", getpid());
                sleep(1);
            }
        }
    }
}

void disconnect_consumer_ipc(int shm_fd, void *shm_ptr, sem_t *sem) {
    sem_close(sem);
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
}
