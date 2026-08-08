#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <time.h> // Добавлено для работы со временем (генератор случайных чисел)

#define SHM_NAME "/eltex_task04_shm"
#define SEM_NAME "/eltex_task04_sem"
#define SHM_SIZE 4096

struct ShmHeader {
    size_t first_block_offset; 
    size_t free_offset;        
    int producer_done;         
};

struct Block {
    size_t num_elements;       
    size_t next_block_offset;  
    int data[];                
};

#endif