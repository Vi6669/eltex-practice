#include "copier.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024

// Вспомогательная функция для работы дочернего процесса (запись файла)
static void run_child_process(int read_fd, const char *src_filename) {
    int fd_out = -1;
    char *dest_filename = NULL;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int status = 0;

    // 1. Формируем имя файла-копии 
    size_t src_len = strlen(src_filename);
    const char *ext = ".copy";
    size_t ext_len = strlen(ext);

    dest_filename = malloc(src_len + ext_len + 1);
    if (dest_filename == NULL) {
        fprintf(stderr, "Child Error: Memory allocation failed for destination filename\n");
        close(read_fd);
        _exit(EXIT_FAILURE); // Дочерний процесс должен завершаться через _exit
    }
    strcpy(dest_filename, src_filename);
    strcat(dest_filename, ext);

    // 2. Создаем файл назначения на запись
    fd_out = open(dest_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        fprintf(stderr, "Child Error: Cannot create destination file '%s': %s\n", 
                dest_filename, strerror(errno));
        free(dest_filename);
        close(read_fd);
        _exit(EXIT_FAILURE);
    }

    // 3. Читаем данные из канала и пишем их в файл-копию
    while ((bytes_read = read(read_fd, buffer, sizeof(buffer))) > 0) {
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
    }

    if (bytes_read < 0) {
        fprintf(stderr, "Child Error: Read failed from pipe: %s\n", strerror(errno));
        status = -1;
    }

    // Освобождаем ресурсы дочернего процесса
    free(dest_filename);
    close(fd_out);
    close(read_fd);

    // Завершаем дочерний процесс
    if (status == 0) {
        _exit(EXIT_SUCCESS);
    } else {
        _exit(EXIT_FAILURE);
    }
}

// Вспомогательная функция для работы родительского процесса (чтение файла)
static int run_parent_process(int write_fd, const char *src_filename, pid_t child_pid) {
    int fd_in = -1;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int status = 0;

    // 1. Открываем исходный файл для чтения
    fd_in = open(src_filename, O_RDONLY);
    if (fd_in < 0) {
        fprintf(stderr, "Parent Error: Cannot open source file '%s': %s\n", 
                src_filename, strerror(errno));
        close(write_fd);
        // Ждем завершения потомка
        waitpid(child_pid, NULL, 0);
        return -1;
    }

    // 2. Читаем исходный файл и пишем данные в канал
    while ((bytes_read = read(fd_in, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_written = 0;
        while (bytes_written < bytes_read) {
            ssize_t written = write(write_fd, buffer + bytes_written, bytes_read - bytes_written);
            if (written < 0) {
                fprintf(stderr, "Parent Error: Write failed to pipe: %s\n", strerror(errno));
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

    // Закрываем наши дескрипторы 
    close(fd_in);
    close(write_fd);

    // 3. Ожидаем завершения дочернего процесса и проверяем его статус
    int child_status;
    if (waitpid(child_pid, &child_status, 0) < 0) {
        fprintf(stderr, "Parent Error: waitpid failed: %s\n", strerror(errno));
        status = -1;
    } else {
        // Проверяем, успешно ли завершился дочерний процесс
        if (WIFEXITED(child_status)) {
            if (WEXITSTATUS(child_status) != 0) {
                status = -1;
            }
        } else {
            status = -1;
        }
    }

    if (status == 0) {
        printf("Successfully copied '%s' via pipe\n", src_filename);
    } else {
        fprintf(stderr, "Failed to copy '%s'\n", src_filename);
    }

    return status;
}

int copy_file(const char *src_filename, const char *fifo_name) {
    // Временно избегаем предупреждения о неиспользуемом параметре
    (void)fifo_name; 

    int pipefd[2];

    // 1. Создаем неименованный канал
    if (pipe(pipefd) < 0) {
        fprintf(stderr, "Error: pipe creation failed: %s\n", strerror(errno));
        return -1;
    }

    // 2. Разветвляем процесс
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Error: fork failed: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        // Дочерний процесс 
        close(pipefd[1]); // Дочерний процесс только читает, закрываем дескриптор записи
        run_child_process(pipefd[0], src_filename);
        
    }

    // Родительский процесс 
    close(pipefd[0]); // Родительский процесс только пишет, закрываем дескриптор чтения
    return run_parent_process(pipefd[1], src_filename, pid);
}