#include "driver.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/timerfd.h>
#include <time.h>

// Чтение состояния таймера водителя через timerfd_gettime
static void check_timer_state(int tfd, int *is_busy, int *rem_time) {
    struct itimerspec curr;
    timerfd_gettime(tfd, &curr);
    *is_busy = (curr.it_value.tv_sec > 0 || curr.it_value.tv_nsec > 0);
    *rem_time = curr.it_value.tv_sec + (curr.it_value.tv_nsec > 0 ? 1 : 0);
}

// Обработка команд от диспетчера
static void process_dispatcher_command(int rsp_fd, int tfd, ipc_msg_t *msg) {
    int is_busy, rem_time;
    check_timer_state(tfd, &is_busy, &rem_time);

    ipc_msg_t reply;
    reply.driver_pid = getpid();

    if (msg->type == MSG_TASK) {
        if (is_busy) {
            reply.type = MSG_BUSY;
            reply.payload = rem_time;
            write(rsp_fd, &reply, sizeof(reply));
        } else {
            // Запуск таймера для имитации работы
            struct itimerspec new_timer = {0};
            new_timer.it_value.tv_sec = msg->payload;
            timerfd_settime(tfd, 0, &new_timer, NULL);
            
            reply.type = MSG_ACK;
            reply.payload = 0;
            write(rsp_fd, &reply, sizeof(reply));
        }
    } 
    else if (msg->type == MSG_STATUS_REQ) {
        reply.type = MSG_STATUS_RSP;
        reply.payload = is_busy ? rem_time : 0;
        write(rsp_fd, &reply, sizeof(reply));
    }
}

// Запуск логики водителя с использованием select()
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

        // Есть сообщение от диспетчера
        if (FD_ISSET(req_fd, &read_fds)) {
            ipc_msg_t msg;
            int r = read(req_fd, &msg, sizeof(msg));
            if (r <= 0) {
                exit(0);
            }
            process_dispatcher_command(rsp_fd, tfd, &msg);
        }
        
        // Сработал таймер окончания задачи
        if (FD_ISSET(tfd, &read_fds)) {
            uint64_t expirations;
            read(tfd, &expirations, sizeof(expirations));
        }
    }
}