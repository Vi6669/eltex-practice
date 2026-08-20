#include "driver.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/timerfd.h>
#include <time.h>

static void check_timer_state(int tfd, int *is_busy, int *rem_time) {
    struct itimerspec curr;
    timerfd_gettime(tfd, &curr);
    *is_busy = (curr.it_value.tv_sec > 0 || curr.it_value.tv_nsec > 0);
    *rem_time = curr.it_value.tv_sec + (curr.it_value.tv_nsec > 0 ? 1 : 0);
}

// Определение типа указателя на функцию-обработчик команд диспетчера
typedef void (*msg_handler_t)(int rsp_fd, int tfd, ipc_msg_t *msg);

// Обработчик команды MSG_TASK (Назначение задачи)
static void handle_task_cmd(int rsp_fd, int tfd, ipc_msg_t *msg) {
    int is_busy, rem_time;
    check_timer_state(tfd, &is_busy, &rem_time);

    ipc_msg_t reply;
    reply.driver_pid = getpid();

    if (is_busy) {
        reply.type = MSG_BUSY;
        reply.payload = rem_time;
    } else {
        struct itimerspec new_timer = {0};
        new_timer.it_value.tv_sec = msg->payload;
        timerfd_settime(tfd, 0, &new_timer, NULL);
        
        reply.type = MSG_ACK;
        reply.payload = 0;
    }
    write(rsp_fd, &reply, sizeof(reply));
}

// Обработчик команды MSG_STATUS_REQ (Запрос статуса)
static void handle_status_req_cmd(int rsp_fd, int tfd, ipc_msg_t *msg) {
    (void)msg;
    int is_busy, rem_time;
    check_timer_state(tfd, &is_busy, &rem_time);

    ipc_msg_t reply;
    reply.driver_pid = getpid();
    reply.type = MSG_STATUS_RSP;
    reply.payload = is_busy ? rem_time : 0;
    
    write(rsp_fd, &reply, sizeof(reply));
}

// Таблица диспетчеризации входящих команд для водителя
static const msg_handler_t driver_handlers[MSG_COUNT] = {
    [MSG_TASK]       = handle_task_cmd,
    [MSG_STATUS_REQ] = handle_status_req_cmd
};

void driver_run(const char *req_fifo_name, const char *rsp_fifo_name) {
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    int req_fd = open(req_fifo_name, O_RDWR | O_NONBLOCK);
    int rsp_fd = open(rsp_fifo_name, O_RDWR | O_NONBLOCK);

    fd_set master_set, read_fds;
    FD_ZERO(&master_set);
    
    FD_SET(req_fd, &master_set);
    FD_SET(tfd, &master_set);
    
    int max_fd = (req_fd > tfd) ? req_fd : tfd;

    while (1) {
        read_fds = master_set;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            break;
        }

        if (FD_ISSET(req_fd, &read_fds)) {
            ipc_msg_t msg;
            int r = read(req_fd, &msg, sizeof(msg));
            if (r <= 0) {
                exit(0);
            }
            
            // Вызов обработчика из таблицы диспетчеризации по типу сообщения
            if (msg.type < MSG_COUNT && driver_handlers[msg.type]) {
                driver_handlers[msg.type](rsp_fd, tfd, &msg);
            }
        }
        
        if (FD_ISSET(tfd, &read_fds)) {
            uint64_t expirations;
            read(tfd, &expirations, sizeof(expirations));
        }
    }
}