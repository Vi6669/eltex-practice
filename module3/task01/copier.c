#include "copier.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024

// Логика работы дочернего процесса 
static void run_child_process(int data_read_fd, int sync_write_fd) {
    int fd_out = -1;
    char *dest_filename = NULL;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int status = 0;

    // 1. Отправляем Родителю сигнал готовности к приему данных ('R')
    char ready_signal = 'R';
    if (write(sync_write_fd, &ready_signal, 1) < 0) {
        fprintf(stderr, "Child Error: Failed to send ready signal: %s\n", strerror(errno));
        close(data_read_fd);
        close(sync_write_fd);
        _exit(EXIT_FAILURE);
    }
    // Канал синхронизации нам больше не нужен, закрываем его
    close(sync_write_fd);

    // 2. Читаем структуру заголовка (метаданные) от Родителя
    struct FileHeader header;
    ssize_t header_bytes = read(data_read_fd, &header, sizeof(header));
    if (header_bytes < (ssize_t)sizeof(header)) {
        fprintf(stderr, "Child Error: Failed to read file header from data pipe\n");
        close(data_read_fd);
        _exit(EXIT_FAILURE);
    }

    // 3. Формируем имя файла-копии на основе полученного заголовка
    size_t src_len = strlen(header.filename);
    const char *ext = ".copy";
    size_t ext_len = strlen(ext);

    dest_filename = malloc(src_len + ext_len + 1);
    if (dest_filename == NULL) {
        fprintf(stderr, "Child Error: Memory allocation failed for destination filename\n");
        close(data_read_fd);
        _exit(EXIT_FAILURE);
    }
    strcpy(dest_filename, header.filename);
    strcat(dest_filename, ext);

    // 4. Создаем файл назначения на запись
    fd_out = open(dest_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        fprintf(stderr, "Child Error: Cannot create destination file '%s': %s\n", 
                dest_filename, strerror(errno));
        free(dest_filename);
        close(data_read_fd);
        _exit(EXIT_FAILURE);
    }

    // 5. Читаем содержимое файла из канала данных и пишем его на диск
    off_t total_written = 0;
    while ((bytes_read = read(data_read_fd, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_written = 0;
        while (bytes_written < bytes_read) {
            ssize_t written = write(fd_out, buffer + bytes_written, bytes_read - bytes_written);
            if (written < 0) {
                fprintf(stderr, "Child Error: Write failed to '%s': %s\n", 
                        dest_filename, strerror(errno));
                status = -1;
                break;
            }
            bytes_written += written;
        }
        if (status < 0) {
            break;
        }
        total_written += bytes_read;
    }

    if (bytes_read < 0) {
        fprintf(stderr, "Child Error: Read failed from data pipe: %s\n", strerror(errno));
        status = -1;
    }

    // Проверяем, совпадает ли реально полученное количество байт с тем, что было в заголовке
    if (status == 0 && total_written != header.file_size) {
        fprintf(stderr, "Child Warning: Received size (%ld) mismatch with expected size (%ld)\n",
                (long)total_written, (long)header.file_size);
    }

    // Освобождаем ресурсы
    free(dest_filename);
    close(fd_out);
    close(data_read_fd);

    if (status == 0) {
        _exit(EXIT_SUCCESS);
    } else {
        _exit(EXIT_FAILURE);
    }
}

// Логика работы родительского процесса 
static int run_parent_process(int data_write_fd, int sync_read_fd, const char *src_filename, pid_t child_pid) {
    int fd_in = -1;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int status = 0;

    // 1. Открываем исходный файл на чтение
    fd_in = open(src_filename, O_RDONLY);
    if (fd_in < 0) {
        fprintf(stderr, "Parent Error: Cannot open source file '%s': %s\n", 
                src_filename, strerror(errno));
        close(data_write_fd);
        close(sync_read_fd);
        waitpid(child_pid, NULL, 0);
        return -1;
    }

    // 2. Получаем размер исходного файла с помощью fstat
    struct stat st;
    if (fstat(fd_in, &st) < 0) {
        fprintf(stderr, "Parent Error: Failed to get file size for '%s': %s\n",
                src_filename, strerror(errno));
        close(fd_in);
        close(data_write_fd);
        close(sync_read_fd);
        waitpid(child_pid, NULL, 0);
        return -1;
    }
    off_t file_size = st.st_size;

    // 3. Ожидаем от Потомка сигнал готовности ('R')
    char ready_signal;
    ssize_t sync_bytes = read(sync_read_fd, &ready_signal, 1);
    if (sync_bytes <= 0 || ready_signal != 'R') {
        fprintf(stderr, "Parent Error: Failed to receive child ready signal\n");
        close(fd_in);
        close(data_write_fd);
        close(sync_read_fd);
        waitpid(child_pid, NULL, 0);
        return -1;
    }
    // Канал синхронизации больше не нужен, закрываем его
    close(sync_read_fd);

    // 4. Формируем и отправляем заголовок файла (метаданные) перед отправкой данных
    struct FileHeader header;
    strncpy(header.filename, src_filename, sizeof(header.filename) - 1);
    header.filename[sizeof(header.filename) - 1] = '\0';
    header.file_size = file_size;

    if (write(data_write_fd, &header, sizeof(header)) < (ssize_t)sizeof(header)) {
        fprintf(stderr, "Parent Error: Failed to write file header to data pipe: %s\n", strerror(errno));
        close(fd_in);
        close(data_write_fd);
        waitpid(child_pid, NULL, 0);
        return -1;
    }

    // 5. Попорционно читаем исходный файл и отправляем его содержимое в канал данных
    while ((bytes_read = read(fd_in, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_written = 0;
        while (bytes_written < bytes_read) {
            ssize_t written = write(data_write_fd, buffer + bytes_written, bytes_read - bytes_written);
            if (written < 0) {
                fprintf(stderr, "Parent Error: Write failed to data pipe: %s\n", strerror(errno));
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
        fprintf(stderr, "Parent Error: Read failed from '%s': %s\n", 
                src_filename, strerror(errno));
        status = -1;
    }

    // Закрытие data_write_fd укажет дочернему процессу на EOF 
    close(fd_in);
    close(data_write_fd);

    // 6. Ожидаем завершения потомка и считываем его статус
    int child_status;
    if (waitpid(child_pid, &child_status, 0) < 0) {
        fprintf(stderr, "Parent Error: waitpid failed: %s\n", strerror(errno));
        status = -1;
    } else {
        if (WIFEXITED(child_status)) {
            if (WEXITSTATUS(child_status) != 0) {
                status = -1;
            }
        } else {
            status = -1;
        }
    }

    if (status == 0) {
        printf("Successfully copied '%s' via pipes with sync\n", src_filename);
    } else {
        fprintf(stderr, "Failed to copy '%s'\n", src_filename);
    }

    return status;
}

int copy_file(const char *src_filename, const char *fifo_name) {
    (void)fifo_name; 

    int pipe_data[2];
    int pipe_sync[2];

    // 1. Создаем первый неименованный канал 
    if (pipe(pipe_data) < 0) {
        fprintf(stderr, "Error: Data pipe creation failed: %s\n", strerror(errno));
        return -1;
    }

    // 2. Создаем второй неименованный канал 
    if (pipe(pipe_sync) < 0) {
        fprintf(stderr, "Error: Sync pipe creation failed: %s\n", strerror(errno));
        close(pipe_data[0]);
        close(pipe_data[1]);
        return -1;
    }

    // 3. Разветвляем процесс
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Error: fork failed: %s\n", strerror(errno));
        close(pipe_data[0]);
        close(pipe_data[1]);
        close(pipe_sync[0]);
        close(pipe_sync[1]);
        return -1;
    }

    if (pid == 0) {
        //  Дочерний процесс 
        close(pipe_data[1]); // Дочерний процесс только читает данные
        close(pipe_sync[0]); // Дочерний процесс только пишет сигналы синхронизации
        
        run_child_process(pipe_data[0], pipe_sync[1]);
        
    }

    //  Родительский процесс 
    close(pipe_data[0]); // Родитель только пишет данные
    close(pipe_sync[1]); // Родитель только читает сигналы синхронизации
    
    return run_parent_process(pipe_data[1], pipe_sync[0], src_filename, pid);
}