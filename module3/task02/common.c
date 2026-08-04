#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>

key_t get_ipc_key(void) {
    key_t key = ftok(QUEUE_PATH, QUEUE_PROJECT_ID);
    if (key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }
    return key;
}

int connect_to_queue(void) {
    key_t key = get_ipc_key();
    int msqid = msgget(key, 0666);
    if (msqid == -1) {
        perror("msgget (ошибка подключения к очереди, проверьте запущен ли брокер)");
        exit(EXIT_FAILURE);
    }
    return msqid;
}