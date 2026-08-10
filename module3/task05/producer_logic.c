#include "producer_logic.h"

int init_producer_ipc(int *shm_fd, sem_t **sem, void **shm_ptr) {
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);

    *shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (*shm_fd == -1) {
        perror("shm_open");
        return -1;
    }

    // Задаем размер (динамически используем дефолтный)
    size_t allocated_size = DEFAULT_SHM_SIZE;
    if (ftruncate(*shm_fd, allocated_size) == -1) {
        perror("ftruncate");
        close(*shm_fd);
        return -1;
    }

    *shm_ptr = mmap(NULL, allocated_size, PROT_READ | PROT_WRITE, MAP_SHARED, *shm_fd, 0);
    if (*shm_ptr == MAP_FAILED) {
        perror("mmap");
        close(*shm_fd);
        return -1;
    }

    *sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (*sem == SEM_FAILED) {
        perror("sem_open");
        munmap(*shm_ptr, allocated_size);
        close(*shm_fd);
        return -1;
    }

    sem_wait(*sem);
    struct ShmHeader *header = (struct ShmHeader *)*shm_ptr;
    header->shm_size = allocated_size; 
    header->first_block_offset = 0;
    header->free_offset = sizeof(struct ShmHeader);
    header->producer_done = 0;
    sem_post(*sem);

    return 0;
}

void run_generation_loop(void *shm_ptr, sem_t *sem) {
    srand(getpid() ^ time(NULL));
    size_t last_block_offset = 0;
    struct ShmHeader *header = (struct ShmHeader *)shm_ptr;

    while (1) {
        usleep(500000);

        sem_wait(sem);

        size_t n = 5 + (rand() % 11);
        size_t block_size = sizeof(struct Block) + n * sizeof(int);

        
        if (header->free_offset + block_size > header->shm_size) {
            printf("\nProducer: Memory limits reached. Stopping data generation.\n");
            header->producer_done = 1;
            sem_post(sem);
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

        sem_post(sem);
    }
}

void wait_for_processing(void *shm_ptr, sem_t *sem) {
    printf("Producer: Waiting for consumers to finish processing...\n");
    struct ShmHeader *header = (struct ShmHeader *)shm_ptr;

    while (1) {
        sleep(1);

        sem_wait(sem);

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
            sem_post(sem);
            break;
        }

        sem_post(sem);
    }
}

void cleanup_producer_ipc(int shm_fd, sem_t *sem, void *shm_ptr) {
    struct ShmHeader *header = (struct ShmHeader *)shm_ptr;
    size_t size = header->shm_size; // Считываем точный размер перед отключением!
    sem_close(sem);
    sem_unlink(SEM_NAME);
    munmap(shm_ptr, size);
    close(shm_fd);
    shm_unlink(SHM_NAME);
    printf("Producer: POSIX IPC resources unlinked. Program terminated.\n");
}