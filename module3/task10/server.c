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

typedef struct {
    pid_t pid;
    int req_fd;
    int rsp_fd;
} driver_info_t;

static driver_info_t drivers[MAX_DRIVERS];
static int num_drivers = 0;

static void print_prompt() {
    printf("> ");
    fflush(stdout);
}

static driver_info_t* find_driver(pid_t pid) {
    for (int i = 0; i < num_drivers; i++) {
        if (drivers[i].pid == pid) return &drivers[i];
    }
    return NULL;
}

// Определение типа указателя на функцию-обработчик ответов от водителя
typedef void (*resp_handler_t)(pid_t pid, int payload);

// Функции-обработчики конкретных типов ответов
static void handle_ack(pid_t pid, int payload) {
    (void)payload;
    printf("[Driver %d] Accepted task.\n", pid);
}

static void handle_busy(pid_t pid, int payload) {
    printf("[Driver %d] Busy %d\n", pid, payload);
}

static void handle_status_rsp(pid_t pid, int payload) {
    if (payload > 0)
        printf("[Driver %d] Busy %d\n", pid, payload);
    else
        printf("[Driver %d] Available\n", pid);
}

// Таблица диспетчеризации ответов от водителей
static const resp_handler_t resp_handlers[MSG_COUNT] = {
    [MSG_ACK]        = handle_ack,
    [MSG_BUSY]       = handle_busy,
    [MSG_STATUS_RSP] = handle_status_rsp
};

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
        int req_fd = open(req_name, O_RDWR | O_NONBLOCK);
        int rsp_fd = open(rsp_name, O_RDWR | O_NONBLOCK);

        drivers[num_drivers].pid = pid;
        drivers[num_drivers].req_fd = req_fd;
        drivers[num_drivers].rsp_fd = rsp_fd;
        num_drivers++;

        FD_SET(rsp_fd, master_set);
        if (rsp_fd > *max_fd) {
            *max_fd = rsp_fd;
        }

        printf("Created driver with PID: %d\n", pid);
    }
}

static void handle_send_task(pid_t pid, int timer) {
    driver_info_t* drv = find_driver(pid);
    if (!drv) {
        printf("Driver %u not found.\n", pid);
        return;
    }
    ipc_msg_t msg = { .type = MSG_TASK, .payload = timer, .driver_pid = 0 };
    write(drv->req_fd, &msg, sizeof(msg));
}

static void handle_get_status(pid_t pid) {
    driver_info_t* drv = find_driver(pid);
    if (!drv) {
        printf("Driver %u not found.\n", pid);
        return;
    }
    ipc_msg_t msg = { .type = MSG_STATUS_REQ, .payload = 0, .driver_pid = 0 };
    write(drv->req_fd, &msg, sizeof(msg));
}

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

static void process_driver_message(int fd) {
    ipc_msg_t msg;
    int r = read(fd, &msg, sizeof(msg));
    if (r <= 0) return;

    pid_t pid = -1;
    for (int i = 0; i < num_drivers; i++) {
        if (drivers[i].rsp_fd == fd) pid = drivers[i].pid;
    }

    // Вызов обработчика из таблицы диспетчеризации по индексу типа сообщения
    if (msg.type < MSG_COUNT && resp_handlers[msg.type]) {
        resp_handlers[msg.type](pid, msg.payload);
    }
}

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

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            process_cli_input(&master_set, &max_fd);
        }

        for (int i = 0; i < num_drivers; i++) {
            if (FD_ISSET(drivers[i].rsp_fd, &read_fds)) {
                process_driver_message(drivers[i].rsp_fd);
            }
        }
    }
}