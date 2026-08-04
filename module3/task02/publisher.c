#include "publisher.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>

void run_publisher(const char *topic) {
    printf("[Издатель] Запуск для темы: '%s'...\n", topic);
    
    int msqid = connect_to_queue();
    printf("[Издатель] Успешно подключен к очереди (ID: %d).\n", msqid);

    // В будущем здесь будет логика отправки сообщений
}