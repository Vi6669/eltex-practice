#include "producer_logic.h"

int init_producer_ipc(key_t key, int *shmid, int *semid, void **shm_ptr) {
    // Каскадное удаление старых зависших ресурсов
    int old_shmid = shmget(key, SHM_SIZE, 0666);
    if (old_shmid != -1) shmctl(old_shmid, IPC_RMID, NULL);
    int old_semid = semget(key, 1, 0666);
    if (old_semid != -1) semctl(old_semid, 0, IPC_RMID);

    // Создание новой разделяемой памяти
    *shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    if (*shmid == -1) {
        perror("shmget");
        return -1;
    }

    // Подключение к разделяемой памяти
    *shm_ptr = shmat(*shmid, NULL, 0);
    if (*shm_ptr == (void *)-1) {
        perror("shmat");
        shmctl(*shmid, IPC_RMID, NULL);
        return -1;
    }

    // Создание семафора
    *semid = semget(key, 1, IPC_CREAT | 0666);
    if (*semid == -1) {
        perror("semget");
        shmdt(*shm_ptr);
        shmctl(*shmid, IPC_RMID, NULL);
        return -1;
    }

    // Инициализация семафора значением 1 (свободен)
    union semun arg;
    arg.val = 1;
    if (semctl(*semid, 0, SETVAL, arg) == -1) {
        perror("semctl SETVAL");
        semctl(*semid, 0, IPC_RMID);
        shmdt(*shm_ptr);
        shmctl(*shmid, IPC_RMID, NULL);
        return -1;
    }

    // Инициализируем заголовок под блокировкой семафора
    struct sembuf lock = {0, -1, 0};
    struct sembuf unlock = {0, 1, 0};

    semop(*semid, &lock, 1);
    struct ShmHeader *header = (struct ShmHeader *)*shm_ptr;
    header->first_block_offset = 0;
    header->free_offset = sizeof(struct ShmHeader);
    header->producer_done = 0;
    semop(*semid, &unlock, 1);

    return 0; // Инициализация успешна
}

void run_generation_loop(void *shm_ptr, int semid) {
    srand(getpid() ^ time(NULL));
    size_t last_block_offset = 0;
    struct ShmHeader *header = (struct ShmHeader *)shm_ptr;

    struct sembuf lock = {0, -1, 0};
    struct sembuf unlock = {0, 1, 0};

    while (1) {
        usleep(500000); // Сон 0.5с

        semop(semid, &lock, 1);

        size_t n = 5 + (rand() % 11);
        size_t block_size = sizeof(struct Block) + n * sizeof(int);

        // Проверка нехватки места
        if (header->free_offset + block_size > SHM_SIZE) {
            printf("\nProducer: Shared memory limits reached. Stopping generation.\n");
            header->producer_done = 1;
            semop(semid, &unlock, 1);
            break;
        }

        size_t new_block_offset = header->free_offset;
        struct Block *new_block = (struct Block *)((char *)shm_ptr + new_block_offset);

        new_block->num_elements = n;
        new_block->next_block_offset = 0;

        printf("Producer generated block at offset %zu with %zu elements: [ ", new_block_offset, n);
        for (size_t i = 0; i < n; i++) {
            new_block->data[i] = rand() % 100;
            printf("%d ", new_block->data[i]);
        }
        printf("]\n");

        if (header->first_block_offset == 0) {
            header->first_block_offset = new_block_offset;
        } else {
            struct Block *prev_block = (struct Block *)((char *)shm_ptr + last_block_offset);
            prev_block->next_block_offset = new_block_offset;
        }

        last_block_offset = new_block_offset;
        header->free_offset += block_size;

        semop(semid, &unlock, 1);
    }
}

void wait_for_processing(void *shm_ptr, int semid) {
    printf("Producer: Waiting for consumers to finish processing all blocks...\n");
    struct ShmHeader *header = (struct ShmHeader *)shm_ptr;

    struct sembuf lock = {0, -1, 0};
    struct sembuf unlock = {0, 1, 0};

    while (1) {
        sleep(1);

        semop(semid, &lock, 1);

        int all_processed = 1;
        size_t current_offset = header->first_block_offset;

        while (current_offset != 0) {
            struct Block *block = (struct Block *)((char *)shm_ptr + current_offset);
            if (block->num_elements > 0) {
                all_processed = 0;
                break;
            }
            current_offset = block->next_block_offset;
        }

        if (all_processed) {
            printf("Producer: All blocks have been processed successfully!\n");
            semop(semid, &unlock, 1);
            break;
        }

        semop(semid, &unlock, 1);
    }
}

void cleanup_producer_ipc(int shmid, int semid, void *shm_ptr) {
    semctl(semid, 0, IPC_RMID);
    shmdt(shm_ptr);
    shmctl(shmid, IPC_RMID, NULL);
    printf("Producer: System V IPC resources unlinked. Program terminated.\n");
}