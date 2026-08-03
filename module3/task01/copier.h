#ifndef COPIER_H
#define COPIER_H

#include <sys/types.h>

// Структура для передачи метаданных файла (заголовка) перед копированием
struct FileHeader {
    char filename[256];  // Имя файла
    off_t file_size;     // Размер файла в байтах
};


int copy_file(const char *src_filename, const char *fifo_name);

#endif // COPIER_H