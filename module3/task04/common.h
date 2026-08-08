#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>

#define FTOK_PATH "/tmp"
#define PROJ_ID 42
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

// Объединение semun для настройки семафора 
#ifdef _SEM_SEMUN_UNDEFINED
union semun {
    int val;                  
    struct semid_ds *buf;     
    unsigned short *array;    
    struct seminfo *__buf;    
};
#endif

#endif /* COMMON_H */