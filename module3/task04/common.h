#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>

#define SHM_NAME "/eltex_task04_shm"
#define SEM_NAME "/eltex_task04_sem"
#define SHM_SIZE 4096  // Небольшой размер для быстрой проверки переполнения

// Заголовок разделяемой памяти
struct ShmHeader {
    size_t first_block_offset; // Смещение первого блока (0, если список пуст)
    size_t free_offset;        // Смещение первой свободной ячейки памяти
    int producer_done;         // Флаг завершения генерации производителем (1 = завершено)
};

// Структура одного блока данных
struct Block {
    size_t num_elements;       // Количество элементов (0 означает, что блок обработан)
    size_t next_block_offset;  // Смещение следующего блока (0, если это последний блок)
    int data[];                // Массив чисел (flexible array member)
};

#endif