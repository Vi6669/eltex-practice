#include "tester.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

int tests_run = 0;
int tests_failed = 0;

// Принудительное удаление IPC ресурсов перед тестом
static void force_cleanup_ipc() {
    key_t key = ftok(FTOK_PATH, PROJ_ID);
    if (key != -1) {
        int shmid = shmget(key, SHM_SIZE, 0666);
        if (shmid != -1) {
            shmctl(shmid, IPC_RMID, NULL);
        }
        int semid = semget(key, 1, 0666);
        if (semid != -1) {
            semctl(semid, 0, IPC_RMID);
        }
    }
}

// Тест 1: Валидность генерации токена ftok
static void test_key_generation() {
    key_t key = ftok(FTOK_PATH, PROJ_ID);
    ASSERT_TRUE("ftok должен успешно генерировать ключ", key != -1);
}

// Тест 2: Инициализация и подключение разделяемой памяти и семафора
static void test_ipc_initialization() {
    force_cleanup_ipc();

    key_t key = ftok(FTOK_PATH, PROJ_ID);
    
    // Создаем память и семафор
    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    ASSERT_TRUE("Разделяемая память должна успешно создаваться", shmid != -1);

    void *shm_ptr = shmat(shmid, NULL, 0);
    ASSERT_TRUE("Разделяемая память должна успешно проецироваться", shm_ptr != (void *)-1);

    int semid = semget(key, 1, IPC_CREAT | 0666);
    ASSERT_TRUE("Семафор должен успешно создаваться", semid != -1);

    // Закрываем и удаляем ресурсы
    shmdt(shm_ptr);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
}

// Тест 3: Граничный случай — взаимное исключение семафора и флаг IPC_NOWAIT
static void test_semaphore_mutual_exclusion() {
    force_cleanup_ipc();

    key_t key = ftok(FTOK_PATH, PROJ_ID);
    int semid = semget(key, 1, IPC_CREAT | 0666);
    
    union semun arg;
    arg.val = 1; // Устанавливаем начальное значение 1 (свободен)
    semctl(semid, 0, SETVAL, arg);

    struct sembuf lock = {0, -1, 0};
    struct sembuf unlock = {0, 1, 0};

    // Блокируем семафор первый раз
    int lock_res = semop(semid, &lock, 1);
    ASSERT_TRUE("Первая блокировка должна пройти успешно", lock_res == 0);

    // Граничный случай: пробуем заблокировать повторно без ожидания (IPC_NOWAIT)
    struct sembuf lock_nowait = {0, -1, IPC_NOWAIT};
    int overlock_res = semop(semid, &lock_nowait, 1);
    
    // Попытка заблокировать запертый семафор должна вернуть ошибку с errno = EAGAIN
    ASSERT_TRUE("Повторная блокировка занятого семафора должна вернуть ошибку", overlock_res == -1);
    ASSERT_TRUE("Код ошибки повторной блокировки должен быть EAGAIN", errno == EAGAIN);

    // Разблокируем обратно
    semop(semid, &unlock, 1);

    // Проверяем, что значение восстановилось
    int val = semctl(semid, 0, GETVAL);
    ASSERT_TRUE("Значение семафора должно вернуться к 1", val == 1);

    semctl(semid, 0, IPC_RMID);
}

// Тест 4: Проверка заголовка и связывания блоков по смещениям (Список)
static void test_block_linking() {
    force_cleanup_ipc();

    key_t key = ftok(FTOK_PATH, PROJ_ID);
    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    void *shm_ptr = shmat(shmid, NULL, 0);

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
    ASSERT_TRUE("Голова списка должна указывать на первый блок", header->first_block_offset == offset1);
    
    struct Block *read_b1 = (struct Block *)((char *)shm_ptr + header->first_block_offset);
    ASSERT_TRUE("Смещение внутри первого блока должно указывать на второй блок", read_b1->next_block_offset == offset2);

    struct Block *read_b2 = (struct Block *)((char *)shm_ptr + read_b1->next_block_offset);
    ASSERT_TRUE("Смещение второго блока должно быть равно 0 (конец списка)", read_b2->next_block_offset == 0);

    shmdt(shm_ptr);
    shmctl(shmid, IPC_RMID, NULL);
}

// Тест 5: Граничный случай — Имитация переполнения памяти (Memory Overflow)
static void test_memory_exhaustion() {
    force_cleanup_ipc();

    key_t key = ftok(FTOK_PATH, PROJ_ID);
    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    void *shm_ptr = shmat(shmid, NULL, 0);

    struct ShmHeader *header = (struct ShmHeader *)shm_ptr;
    header->first_block_offset = 0;
    // Смещаем free_offset близко к лимиту SHM_SIZE
    header->free_offset = SHM_SIZE - 20; 
    header->producer_done = 0;

    // Пытаемся выделить блок, который требует 40 байт (он точно не поместится)
    size_t block_size = sizeof(struct Block) + 5 * sizeof(int); // ~24 + 20 = 44 байта
    
    int space_exhausted = 0;
    if (header->free_offset + block_size > SHM_SIZE) {
        space_exhausted = 1;
        header->producer_done = 1;
    }

    ASSERT_TRUE("Логика переполнения должна зафиксировать нехватку памяти", space_exhausted == 1);
    ASSERT_TRUE("При переполнении должен выставляться флаг producer_done = 1", header->producer_done == 1);

    shmdt(shm_ptr);
    shmctl(shmid, IPC_RMID, NULL);
}

// Тест 6: Алгоритм разбора данных потребителем и сброс количества элементов
static void test_consumer_processing_logic() {
    force_cleanup_ipc();

    key_t key = ftok(FTOK_PATH, PROJ_ID);
    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    void *shm_ptr = shmat(shmid, NULL, 0);

    struct ShmHeader *header = (struct ShmHeader *)shm_ptr;
    header->first_block_offset = header->free_offset = sizeof(struct ShmHeader);

    // Записываем тестовый блок с отрицательными, положительными числами и нулями
    struct Block *b = (struct Block *)((char *)shm_ptr + header->first_block_offset);
    b->num_elements = 5;
    b->next_block_offset = 0;
    b->data[0] = 10;
    b->data[1] = -42; // Отрицательное число (граничное значение для минимума)
    b->data[2] = 100; // Граничное значение для максимума
    b->data[3] = 0;
    b->data[4] = 42;

    // Симулируем чтение и поиск min/max потребителем
    size_t n = b->num_elements;
    int min = b->data[0];
    int max = b->data[0];

    for (size_t i = 0; i < n; i++) {
        int val = b->data[i];
        if (val < min) min = val;
        if (val > max) max = val;
    }

    // Маркируем блок как обработанный
    b->num_elements = 0;

    ASSERT_TRUE("Потребитель должен правильно вычислить минимум (-42)", min == -42);
    ASSERT_TRUE("Потребитель должен правильно вычислить максимум (100)", max == 100);
    ASSERT_TRUE("После обработки количество элементов должно быть сброшено в 0", b->num_elements == 0);

    shmdt(shm_ptr);
    shmctl(shmid, IPC_RMID, NULL);
}

int main() {
    printf("Запуск автоматических модульных тестов System V: ");
    fflush(stdout);

    RUN_TEST(test_key_generation);
    RUN_TEST(test_ipc_initialization);
    RUN_TEST(test_semaphore_mutual_exclusion);
    RUN_TEST(test_block_linking);
    RUN_TEST(test_memory_exhaustion);
    RUN_TEST(test_consumer_processing_logic);

    // Окончательная очистка ресурсов после завершения тестов
    force_cleanup_ipc();

    printf("\nЗапущено тестов: %d, Ошибок: %d\n", tests_run, tests_failed);

    if (tests_failed > 0) {
        printf("[РЕЗУЛЬТАТ]: Тесты провалены!\n");
        return EXIT_FAILURE;
    }
    printf("[РЕЗУЛЬТАТ]: Все тесты успешно пройдены!\n");
    return EXIT_SUCCESS;
}