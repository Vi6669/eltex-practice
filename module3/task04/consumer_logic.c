#include "consumer_logic.h"

int connect_consumer_ipc(key_t key, int *shmid, int *semid, void **shm_ptr) {
    // Подключаемся к разделяемой памяти
    *shmid = shmget(key, SHM_SIZE, 0666);
    if (*shmid == -1) {
        perror("shmget (is producer running?)");
        return -1;
    }

    // Отображаем разделяемую память
    *shm_ptr = shmat(*shmid, NULL, 0);
    if (*shm_ptr == (void *)-1) {
        perror("shmat");
        return -1;
    }

    // Подключаемся к семафору
    *semid = semget(key, 1, 0666);
    if (*semid == -1) {
        perror("semget");
        shmdt(*shm_ptr);
        return -1;
    }

    return 0;
}

void run_consumer_loop(void *shm_ptr, int semid) {
    struct sembuf lock = {0, -1, 0};
    struct sembuf unlock = {0, 1, 0};

    while (1) {
        semop(semid, &lock, 1);

        struct ShmHeader *header = (struct ShmHeader *)shm_ptr;
        size_t current_offset = header->first_block_offset;
        struct Block *block_to_process = NULL;
        size_t block_offset = 0;

        // Поиск первого необработанного блока
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

            semop(semid, &unlock, 1);

            // Спим некоторое время после обработки
            sleep(2);
        } else {
            if (header->producer_done) {
                printf("Consumer [%d]: All blocks processed and Producer is done. Exiting loop.\n", getpid());
                semop(semid, &unlock, 1);
                break;
            } else {
                semop(semid, &unlock, 1);
                printf("Consumer [%d]: No unprocessed blocks. Waiting...\n", getpid());
                sleep(1);
            }
        }
    }
}

void disconnect_consumer_ipc(void *shm_ptr) {
    shmdt(shm_ptr);
}