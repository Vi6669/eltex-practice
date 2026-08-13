#include "server.h"
#include "common.h"
#include "driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/select.h>

#define MAX_DRIVERS 100

// Структура для учета водителей диспетчером
typedef struct {
    pid_t pid;          // PID процесса водителя
    int req_fd;         // Дескриптор отправки запросов (запись)
    int rsp_fd;         // Дескриптор приема ответов (чтение)
} driver_info_t;

static driver_info_t drivers[MAX_DRIVERS];
static int num_drivers = 0;

static void print_prompt() {
    printf("> ");
    fflush(stdout);
}

// Поиск водителя в локальном массиве по PID
static driver_info_t* find_driver(pid_t pid) {
    for (int i = 0; i < num_drivers; i++) {
        if (drivers[i].pid == pid) return &drivers[i];
    }
    return NULL;
}

// Создание нового процесса водителя и его FIFO
static void handle_create_driver(fd_set *master_set, int *max_fd) {
    if (num_drivers >= MAX_DRIVERS) return;

    char req_name[64], rsp_name[64];
    sprintf(req_name, "/tmp/taxi_req_%d", num_drivers);
    sprintf(rsp_name, "/tmp/taxi_rsp_%d", num_drivers);

    mkfifo(req_name, 0666);
    mkfifo(rsp_name, 0666);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        driver_run(req_name, rsp_name);
        exit(0);
    } else {
        // O_RDWR предотвращает блокировку при открытии неактивного FIFO
        int req_fd = open(req_name, O_RDWR | O_NONBLOCK);
        int rsp_fd = open(rsp_name, O_RDWR | O_NONBLOCK);

        drivers[num_drivers].pid = pid;
        drivers[num_drivers].req_fd = req_fd;
        drivers[num_drivers].rsp_fd = rsp_fd;
        num_drivers++;

        // Добавляем новый дескриптор чтения в набор select
        FD_SET(rsp_fd, master_set);
        if (rsp_fd > *max_fd) {
            *max_fd = rsp_fd;
        }

        printf("Created driver with PID: %d\n", pid);
    }
}

// Отправка задачи водителю
static void handle_send_task(pid_t pid, int timer) {
    driver_info_t* drv = find_driver(pid);
    if (!drv) {
        printf("Driver %u not found.\n", pid);
        return;
    }
    ipc_msg_t msg = { .type = MSG_TASK, .payload = timer, .driver_pid = 0 };
    write(drv->req_fd, &msg, sizeof(msg));
}

// Запрос статуса водителя
static void handle_get_status(pid_t pid) {
    driver_info_t* drv = find_driver(pid);
    if (!drv) {
        printf("Driver %u not found.\n", pid);
        return;
    }
    ipc_msg_t msg = { .type = MSG_STATUS_REQ, .payload = 0, .driver_pid = 0 };
    write(drv->req_fd, &msg, sizeof(msg));
}

// Запрос статуса у всех водителей
static void handle_get_drivers() {
    if (num_drivers == 0) {
        printf("No drivers running.\n");
        return;
    }
    ipc_msg_t msg = { .type = MSG_STATUS_REQ, .payload = 0, .driver_pid = 0 };
    for (int i = 0; i < num_drivers; i++) {
        write(drivers[i].req_fd, &msg, sizeof(msg));
    }
}

// Парсинг ввода CLI
static void process_cli_input(fd_set *master_set, int *max_fd) {
    char buf[256];
    if (!fgets(buf, sizeof(buf), stdin)) return;

    char cmd[64];
    int pid, timer;

    if (sscanf(buf, "%63s", cmd) != 1) {
        print_prompt();
        return;
    }

    if (strcmp(cmd, "create_driver") == 0) {
        handle_create_driver(master_set, max_fd);
    } 
    else if (strcmp(cmd, "send_task") == 0) {
        if (sscanf(buf, "%*s %d %d", &pid, &timer) == 2) {
            handle_send_task(pid, timer);
        } else {
            printf("Usage: send_task <pid> <task_timer>\n");
        }
    } 
    else if (strcmp(cmd, "get_status") == 0) {
        if (sscanf(buf, "%*s %d", &pid) == 1) {
            handle_get_status(pid);
        } else {
            printf("Usage: get_status <pid>\n");
        }
    } 
    else if (strcmp(cmd, "get_drivers") == 0) {
        handle_get_drivers();
    } 
    else if (strcmp(cmd, "exit") == 0) {
        system("rm -f /tmp/taxi_req_* /tmp/taxi_rsp_*");
        exit(0);
    } 
    else {
        printf("Unknown command.\n");
    }
    print_prompt();
}

// Обработка входящего ответа от водителя
static void process_driver_message(int fd) {
    ipc_msg_t msg;
    int r = read(fd, &msg, sizeof(msg));
    if (r <= 0) return;

    printf("\n");
    switch (msg.type) {
        case MSG_ACK:
            printf("[Driver %d] Accepted task.\n", msg.driver_pid);
            break;
        case MSG_BUSY:
            printf("[Driver %d] Busy %d\n", msg.driver_pid, msg.payload);
            break;
        case MSG_STATUS_RSP:
            if (msg.payload > 0)
                printf("[Driver %d] Busy %d\n", msg.driver_pid, msg.payload);
            else
                printf("[Driver %d] Available\n", msg.driver_pid);
            break;
        default:
            break;
    }
    print_prompt();
}

// Диспетчерский цикл на основе select()
void server_run(void) {
    fd_set master_set, read_fds;
    int max_fd = STDIN_FILENO;

    FD_ZERO(&master_set);
    FD_SET(STDIN_FILENO, &master_set);

    print_prompt();

    while (1) {
        read_fds = master_set;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            break;
        }

        // Проверяем ввод пользователя
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            process_cli_input(&master_set, &max_fd);
        }

        // Проверяем ответы от водителей
        for (int i = 0; i < num_drivers; i++) {
            if (FD_ISSET(drivers[i].rsp_fd, &read_fds)) {
                process_driver_message(drivers[i].rsp_fd);
            }
        }
    }
}