#include "subscriber.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void print_subscribed_topics(int count, char **topics) {
    printf("[Подписчик] Список отслеживаемых тем:\n");
    for (int i = 0; i < count; i++) {
        printf("  - %s\n", topics[i]);
    }
}

void run_subscriber(int topic_count, char **topics) {
    printf("[Подписчик] Запуск...\n");
    print_subscribed_topics(topic_count, topics);

    int msqid = connect_to_queue();
    printf("[Подписчик] Успешно подключен к очереди (ID: %d). Мой PID: %d\n", msqid, getpid());

    // В будущем здесь будет логика подписки и чтения сообщений
}