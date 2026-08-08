
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <time.h>

#define SHM_NAME "/eltex_task05_shm"
#define SEM_NAME "/eltex_task05_sem"
#define SHM_SIZE 4096

// Заголовок разделяемой памяти
struct ShmHeader {
    size_t first_block_offset; 
    size_t free_offset;        
    int producer_done;         
};

// Структура одного блока данных
struct Block {
    size_t num_elements;       
    size_t next_block_offset;  
    int data[];                
};

#endif /* COMMON_H */
