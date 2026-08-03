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
static void run_child_process(int data_read_fd, int sync_write_fd, const char *src_filename) {
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
    close(sync_write_fd); // Сигнал отправлен, закрываем дескриптор записи синхронизации

    // 2. Читаем структуру заголовка (метаданные) от Родителя
    struct FileHeader header;
    ssize_t header_bytes = read(data_read_fd, &header, sizeof(header));
    if (header_bytes < (ssize_t)sizeof(header)) {
        fprintf(stderr, "Child Error: Failed to read file header from data channel\n");
        close(data_read_fd);
        _exit(EXIT_FAILURE);
    }

    // 3. Формируем имя файла-копии на основе заголовка. 
    
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
        fprintf(stderr, "Child Error: Read failed from data channel: %s\n", strerror(errno));
        status = -1;
    }

    // Сверяем размер переданных данных с заголовком
    if (status == 0 && total_written != header.file_size) {
        fprintf(stderr, "Child Warning: Received size (%ld) mismatch with expected size (%ld) for '%s'\n",
                (long)total_written, (long)header.file_size, src_filename);
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

    // 2. Получаем размер исходного файла 
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
        fprintf(stderr, "Parent Error: Failed to receive child ready signal for '%s'\n", src_filename);
        close(fd_in);
        close(data_write_fd);
        close(sync_read_fd);
        waitpid(child_pid, NULL, 0);
        return -1;
    }
    close(sync_read_fd); // Дескриптор чтения синхронизации больше не нужен, закрываем его

    // 4. Формируем и отправляем заголовок файла (метаданные)
    struct FileHeader header;
    strncpy(header.filename, src_filename, sizeof(header.filename) - 1);
    header.filename[sizeof(header.filename) - 1] = '\0';
    header.file_size = file_size;

    if (write(data_write_fd, &header, sizeof(header)) < (ssize_t)sizeof(header)) {
        fprintf(stderr, "Parent Error: Failed to write file header to data channel: %s\n", strerror(errno));
        close(fd_in);
        close(data_write_fd);
        waitpid(child_pid, NULL, 0);
        return -1;
    }

    // 5. Читаем исходный файл и пишем его данные в канал данных
    while ((bytes_read = read(fd_in, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_written = 0;
        while (bytes_written < bytes_read) {
            ssize_t written = write(data_write_fd, buffer + bytes_written, bytes_read - bytes_written);
            if (written < 0) {
                fprintf(stderr, "Parent Error: Write failed to data channel: %s\n", strerror(errno));
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

    close(fd_in);
    close(data_write_fd); // Закрытие data_write_fd укажет дочернему процессу на EOF 

    // 6. Ожидаем завершения потомка
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
        printf("Successfully copied '%s'\n", src_filename);
    } else {
        fprintf(stderr, "Failed to copy '%s'\n", src_filename);
    }

    return status;
}

int copy_file(const char *src_filename, const char *fifo_name) {
    int data_fd = -1;
    int sync_fd = -1;

    char fifo_data_path[512] = {0};
    char fifo_sync_path[512] = {0};

    // 1. Создаем каналы в зависимости от переданного fifo_name
    if (fifo_name != NULL) {
        //  Режим именованных каналов (FIFO) 
        snprintf(fifo_data_path, sizeof(fifo_data_path), "%s_data", fifo_name);
        snprintf(fifo_sync_path, sizeof(fifo_sync_path), "%s_sync", fifo_name);

        // Удаляем старые файлы FIFO, если они остались в системе
        unlink(fifo_data_path);
        unlink(fifo_sync_path);

        // Создаем два именованных канала
        if (mkfifo(fifo_data_path, 0666) < 0) {
            fprintf(stderr, "Error: Failed to create data FIFO '%s': %s\n", fifo_data_path, strerror(errno));
            return -1;
        }
        if (mkfifo(fifo_sync_path, 0666) < 0) {
            fprintf(stderr, "Error: Failed to create sync FIFO '%s': %s\n", fifo_sync_path, strerror(errno));
            unlink(fifo_data_path);
            return -1;
        }
    } else {
        //  Режим неименованных каналов (pipe) 
        int pipe_data[2];
        int pipe_sync[2];

        if (pipe(pipe_data) < 0) {
            fprintf(stderr, "Error: Data pipe creation failed: %s\n", strerror(errno));
            return -1;
        }
        if (pipe(pipe_sync) < 0) {
            fprintf(stderr, "Error: Sync pipe creation failed: %s\n", strerror(errno));
            close(pipe_data[0]);
            close(pipe_data[1]);
            return -1;
        }

        // Временно сохраняем дескрипторы для передачи
        data_fd = pipe_data[0]; // Чтение данных
        sync_fd = pipe_sync[1]; // Запись синхронизации (для передачи потомку)
    }

    // 2. Разветвляем процесс
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Error: fork failed: %s\n", strerror(errno));
        if (fifo_name != NULL) {
            unlink(fifo_data_path);
            unlink(fifo_sync_path);
        } else {
            // Закрываем дескрипторы pipe
            close(data_fd); // Это pipe_data[0]
            close(sync_fd); // Это pipe_sync[1]
            
        }
        return -1;
    }

    if (pid == 0) {
        //  Дочерний процесс 
        int child_data_fd = -1;
        int child_sync_fd = -1;

        if (fifo_name != NULL) {
            // 1. Открываем канал синхронизации на запись (блокируется, пока Родитель не откроет его на чтение)
            child_sync_fd = open(fifo_sync_path, O_WRONLY);
            if (child_sync_fd < 0) {
                fprintf(stderr, "Child Error: Failed to open sync FIFO '%s': %s\n", fifo_sync_path, strerror(errno));
                _exit(EXIT_FAILURE);
            }
            // 2. Открываем канал данных на чтение (блокируется, пока Родитель не откроет его на запись)
            child_data_fd = open(fifo_data_path, O_RDONLY);
            if (child_data_fd < 0) {
                fprintf(stderr, "Child Error: Failed to open data FIFO '%s': %s\n", fifo_data_path, strerror(errno));
                close(child_sync_fd);
                _exit(EXIT_FAILURE);
            }
        } else {
            
            child_data_fd = data_fd;
            child_sync_fd = sync_fd;
            
            
            close(child_data_fd + 1); // pipe_data[1]
            close(child_sync_fd - 1); // pipe_sync[0]
        }

        run_child_process(child_data_fd, child_sync_fd, src_filename);
    }

    //  Родительский процесс 
    int parent_data_fd = -1;
    int parent_sync_fd = -1;
    int status = 0;

    if (fifo_name != NULL) {
        // 1. Открываем канал синхронизации на чтение (разблокирует Потомок на шаге open)
        parent_sync_fd = open(fifo_sync_path, O_RDONLY);
        if (parent_sync_fd < 0) {
            fprintf(stderr, "Parent Error: Failed to open sync FIFO '%s': %s\n", fifo_sync_path, strerror(errno));
            waitpid(pid, NULL, 0);
            unlink(fifo_data_path);
            unlink(fifo_sync_path);
            return -1;
        }
        // 2. Открываем канал данных на запись (разблокирует Потомок на шаге open)
        parent_data_fd = open(fifo_data_path, O_WRONLY);
        if (parent_data_fd < 0) {
            fprintf(stderr, "Parent Error: Failed to open data FIFO '%s': %s\n", fifo_data_path, strerror(errno));
            close(parent_sync_fd);
            waitpid(pid, NULL, 0);
            unlink(fifo_data_path);
            unlink(fifo_sync_path);
            return -1;
        }
    } else {
        
        parent_data_fd = data_fd + 1; // pipe_data[1] (запись данных)
        parent_sync_fd = sync_fd - 1; // pipe_sync[0] (чтение синхронизации)

        // Закрываем неиспользуемые родителем унаследованные концы:
        close(data_fd); // pipe_data[0]
        close(sync_fd); // pipe_sync[1]
    }

    // Запускаем работу родительского процесса
    status = run_parent_process(parent_data_fd, parent_sync_fd, src_filename, pid);

    // Удаляем файлы именованных каналов после окончания копирования этого файла
    if (fifo_name != NULL) {
        unlink(fifo_data_path);
        unlink(fifo_sync_path);
    }

    return status;
}