#include "copier.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define BUFFER_SIZE 1024

int copy_file_single_process(const char *src_filename) {
    int fd_in = -1;
    int fd_out = -1;
    char *dest_filename = NULL;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int status = 0;

    // 1. Пытаемся открыть исходный файл для чтения
    fd_in = open(src_filename, O_RDONLY);
    if (fd_in < 0) {
        fprintf(stderr, "Error: Cannot open source file '%s': %s\n", 
                src_filename, strerror(errno));
        return -1;
    }

    // 2. Генерируем имя целевого файла 
    size_t src_len = strlen(src_filename);
    const char *ext = ".copy";
    size_t ext_len = strlen(ext);

    dest_filename = malloc(src_len + ext_len + 1);
    if (dest_filename == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for destination filename\n");
        close(fd_in);
        return -1;
    }
    strcpy(dest_filename, src_filename);
    strcat(dest_filename, ext);

    // 3. Создаем/открываем файл назначения для записи
    
    fd_out = open(dest_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        fprintf(stderr, "Error: Cannot create destination file '%s': %s\n", 
                dest_filename, strerror(errno));
        free(dest_filename);
        close(fd_in);
        return -1;
    }

    // 4. Цикл попорционного чтения и записи 
    while ((bytes_read = read(fd_in, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_written = 0;
        
        // Цикл для гарантированной обработки частичной записи 
        while (bytes_written < bytes_read) {
            ssize_t written = write(fd_out, buffer + bytes_written, bytes_read - bytes_written);
            if (written < 0) {
                fprintf(stderr, "Error: Write failed to '%s': %s\n", 
                        dest_filename, strerror(errno));
                status = -1;
                break;
            }
            bytes_written += written;
        }
        
        if (status < 0) {
            break;
        }
    }

    if (bytes_read < 0) {
        fprintf(stderr, "Error: Read failed from '%s': %s\n", 
                src_filename, strerror(errno));
        status = -1;
    }

    if (status == 0) {
        printf("Successfully copied '%s' to '%s'\n", src_filename, dest_filename);
    }

    // 5. Очистка выделенных ресурсов и закрытие дескрипторов
    free(dest_filename);
    close(fd_in);
    close(fd_out);

    return status;
}