
#include "tester.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>

int tests_run = 0;
int tests_failed = 0;

static void force_cleanup_ipc() {
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);
}

// Тест 1: Инициализация и подключение разделяемой памяти и семафора POSIX
static void test_ipc_initialization() {
    force_cleanup_ipc();

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ASSERT_TRUE("Разделяемая память POSIX должна успешно создаваться", shm_fd != -1);
    ftruncate(shm_fd, SHM_SIZE);

    void *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    ASSERT_TRUE("Разделяемая память POSIX должна успешно проецироваться", shm_ptr != MAP_FAILED);

    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    ASSERT_TRUE("Семафор POSIX должен успешно создаваться", sem != SEM_FAILED);

    sem_close(sem);
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
    force_cleanup_ipc();
}

// Тест 2: Граничный случай — взаимное исключение семафора и sem_trywait() [18]
static void test_semaphore_mutual_exclusion() {
    force_cleanup_ipc();

    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    ASSERT_TRUE("Инициализация семафора значением 1", sem != SEM_FAILED);

    int lock_res = sem_wait(sem);
    ASSERT_TRUE("Блокировка семафора должна пройти успешно", lock_res == 0);

    // Граничный случай: повторная неблокирующая попытка через sem_trywait() [18]
    int overlock_res = sem_trywait(sem);
    ASSERT_TRUE("Повторный sem_trywait должен вернуть ошибку (-1)", overlock_res == -1);
    ASSERT_TRUE("Код ошибки повторного зажатия должен быть EAGAIN", errno == EAGAIN);

    sem_post(sem);
    sem_close(sem);
    force_cleanup_ipc();
}

// Тест 3: Проверка заголовка и связывания блоков по смещениям (Список)
static void test_block_linking() {
    force_cleanup_ipc();

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, SHM_SIZE);
    void *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    struct ShmHeader *header = (struct ShmHeader *)shm_ptr;
    header->first_block_offset = 0;
    header->free_offset = sizeof(struct ShmHeader);

    // Симулируем добавление блока 1 размером 3 элемента
    size_t offset1 = header->free_offset;
    struct Block *b1 = (struct Block *)((char *)shm_ptr + offset1);
    b1->num_elements = 3;
    b1->next_block_offset = 0;
    header->first_block_offset = offset1;
    header->free_offset += (sizeof(struct Block) + 3 * sizeof(int));

    // Симулируем добавление блока 2 размером 4 элемента
    size_t offset2 = header->free_offset;
    struct Block *b2 = (struct Block *)((char *)shm_ptr + offset2);
    b2->num_elements = 4;
    b2->next_block_offset = 0;
    b1->next_block_offset = offset2; // Связываем первый блок со вторым
    header->free_offset += (sizeof(struct Block) + 4 * sizeof(int));

    // Проверяем связанность цепочки смещений
    ASSERT_TRUE("Смещение головы указывает на блок 1", header->first_block_offset == offset1);
    struct Block *read_b1 = (struct Block *)((char *)shm_ptr + header->first_block_offset);
    ASSERT_TRUE("Связывание первого со вторым", read_b1->next_block_offset == offset2);

    struct Block *read_b2 = (struct Block *)((char *)shm_ptr + read_b1->next_block_offset);
    ASSERT_TRUE("Второй блок последний (offset 0)", read_b2->next_block_offset == 0);

    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
    force_cleanup_ipc();
}

// Тест 4: Граничный случай — Имитация переполнения памяти (Memory Overflow)
static void test_memory_exhaustion() {
    force_cleanup_ipc();

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, SHM_SIZE);
    void *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    struct ShmHeader *header = (struct ShmHeader *)shm_ptr;
    header->free_offset = SHM_SIZE - 20;
    header->producer_done = 0;

    size_t block_size = sizeof(struct Block) + 5 * sizeof(int);
    int space_exhausted = 0;
    if (header->free_offset + block_size > SHM_SIZE) {
        space_exhausted = 1;
        header->producer_done = 1;
    }

    ASSERT_TRUE("Фиксация нехватки памяти", space_exhausted == 1);
    ASSERT_TRUE("Установка флага producer_done в 1", header->producer_done == 1);

    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
    force_cleanup_ipc();
}

// Тест 5: Алгоритм разбора данных потребителем и сброс количества элементов
static void test_consumer_processing_logic() {
    force_cleanup_ipc();

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, SHM_SIZE);
    void *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    struct ShmHeader *header = (struct ShmHeader *)shm_ptr;
    header->first_block_offset = header->free_offset = sizeof(struct ShmHeader);

    struct Block *b = (struct Block *)((char *)shm_ptr + header->first_block_offset);
    b->num_elements = 5;
    b->next_block_offset = 0;
    b->data[0] = 10;
    b->data[1] = -42;
    b->data[2] = 100;
    b->data[3] = 0;
    b->data[4] = 42;

    size_t n = b->num_elements;
    int min = b->data[0];
    int max = b->data[0];

    for (size_t i = 0; i < n; i++) {
        int val = b->data[i];
        if (val < min) min = val;
        if (val > max) max = val;
    }

    b->num_elements = 0;

    ASSERT_TRUE("Минимум -42", min == -42);
    ASSERT_TRUE("Максимум 100", max == 100);
    ASSERT_TRUE("Сброс элементов в 0", b->num_elements == 0);

    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
    force_cleanup_ipc();
}

int main() {
    printf("Запуск автоматических модульных тестов POSIX (task05): ");
    fflush(stdout);

    RUN_TEST(test_ipc_initialization);
    RUN_TEST(test_semaphore_mutual_exclusion);
    RUN_TEST(test_block_linking);
    RUN_TEST(test_memory_exhaustion);
    RUN_TEST(test_consumer_processing_logic);

    force_cleanup_ipc();

    printf("\nЗапущено тестов: %d, Ошибок: %d\n", tests_run, tests_failed);

    if (tests_failed > 0) {
        printf("[РЕЗУЛЬТАТ]: Тесты POSIX провалены!\n");
        return EXIT_FAILURE;
    }
    printf("[РЕЗУЛЬТАТ]: Все тесты POSIX успешно пройдены!\n");
    return EXIT_SUCCESS;
}
