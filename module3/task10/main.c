#include "server.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // Удаление возможных старых FIFO от прошлых сессий
    system("rm -f /tmp/taxi_req_* /tmp/taxi_rsp_*");

    printf("--- Taxi System (FIFOs + select) ---\n");
    printf("Commands: create_driver, send_task <pid> <sec>, get_status <pid>, get_drivers, exit\n");
    
    server_run();
    return 0;
}