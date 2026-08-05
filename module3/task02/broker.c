#include "broker.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <errno.h>
#include <unistd.h>

#define MAX_SUBSCRIBERS 128
#define MAX_PUBLISHERS 128

typedef struct {
    pid_t pid;
    char topic[64];
} Subscriber;

// Хранилище активных подписчиков и издателей
static Subscriber subscribers[MAX_SUBSCRIBERS];
static int subscriber_count = 0;

static pid_t publishers[MAX_PUBLISHERS];
static int publisher_count = 0;

// Вспомогательная функция создания эксклюзивной очереди
static int create_exclusive_queue(void) {
    key_t key = get_ipc_key();
    int msqid = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    if (msqid == -1) {
        if (errno == EEXIST) {
            fprintf(stderr, "[Брокер] Ошибка: Очередь сообщений уже создана другим брокером.\n");
            exit(EXIT_FAILURE);
        } else {
            perror("msgget (ошибка создания очереди)");
            exit(EXIT_FAILURE);
        }
    }
    return msqid;
}

// Регистрация нового подписчика на конкретную тему
static void register_subscriber(pid_t pid, const char *topic) {
    if (subscriber_count >= MAX_SUBSCRIBERS) {
        fprintf(stderr, "[Брокер] Превышен лимит подписчиков!\n");
        return;
    }
    // Проверка дубликатов (чтобы не подписывать один PID на одну тему дважды)
    for (int i = 0; i < subscriber_count; i++) {
        if (subscribers[i].pid == pid && strcmp(subscribers[i].topic, topic) == 0) {
            return; 
        }
    }
    subscribers[subscriber_count].pid = pid;
    strncpy(subscribers[subscriber_count].topic, topic, 63);
    subscribers[subscriber_count].topic[63] = '\0';
    subscriber_count++;
    printf("[Брокер] Зарегистрирован подписчик %d на тему '%s'\n", pid, topic);
}

// Регистрация издателя в списке активных издателей
static void register_publisher(pid_t pid) {
    if (publisher_count >= MAX_PUBLISHERS) {
        return;
    }
    for (int i = 0; i < publisher_count; i++) {
        if (publishers[i] == pid) {
            return; // Уже зарегистрирован
        }
    }
    publishers[publisher_count] = pid;
    publisher_count++;
    printf("[Брокер] Добавлен издатель %d в список рассылки сигналов\n", pid);
}

// Маршрутизация (рассылка) сообщения подписчикам темы
static void route_message(int msqid, const char *topic, const char *payload) {
    struct msgbuf out_msg;
    // Формируем текст сообщения для подписчика, указывая тему
    snprintf(out_msg.mtext, sizeof(out_msg.mtext), "[%s]: %s", topic, payload);

    int routed_count = 0;
    for (int i = 0; i < subscriber_count; i++) {
        if (strcmp(subscribers[i].topic, topic) == 0) {
            out_msg.mtype = subscribers[i].pid; // Адресуем конкретному подписчику
            
            if (msgsnd(msqid, &out_msg, strlen(out_msg.mtext) + 1, 0) == -1) {
                perror("[Брокер] Ошибка отправки подписчику");
            } else {
                printf("[Брокер] Сообщение переслано подписчику %d\n", subscribers[i].pid);
                routed_count++;
            }
        }
    }
    if (routed_count == 0) {
        printf("[Брокер] Сообщение в тему '%s' никем не отслеживается.\n", topic);
    }
}

void run_broker(void) {
    printf("[Брокер] Запуск...\n");
    
    int msqid = create_exclusive_queue();
    printf("[Брокер] Очередь сообщений успешно создана (ID: %d).\n", msqid);

    struct msgbuf buf;
    while (1) {
        // Считываем сообщения только с приоритетом (mtype) = 1
        if (msgrcv(msqid, &buf, sizeof(buf.mtext), 1, 0) == -1) {
            perror("[Брокер] Ошибка msgrcv");
            break;
        }

        // Парсинг сообщений типа "subscribe,PID,TOPIC"
        if (strncmp(buf.mtext, "subscribe,", 10) == 0) {
            pid_t sub_pid;
            char topic[64];
            if (sscanf(buf.mtext, "subscribe,%d,%63s", &sub_pid, topic) == 2) {
                register_subscriber(sub_pid, topic);
            }
        } 
        // Парсинг сообщений типа "send,PID,TOPIC,PAYLOAD"
        else if (strncmp(buf.mtext, "send,", 5) == 0) {
            pid_t pub_pid;
            char topic[64];
            
            // Безопасный парсинг разделителей ','
            char *comma1 = strchr(buf.mtext, ','); // после "send"
            if (comma1) {
                char *comma2 = strchr(comma1 + 1, ','); // после pid
                if (comma2) {
                    char *comma3 = strchr(comma2 + 1, ','); // после topic
                    if (comma3) {
                        pub_pid = atoi(comma1 + 1);
                        
                        int topic_len = comma3 - (comma2 + 1);
                        if (topic_len > 63) topic_len = 63;
                        strncpy(topic, comma2 + 1, topic_len);
                        topic[topic_len] = '\0';
                        
                        const char *payload = comma3 + 1;
                        
                        register_publisher(pub_pid);
                        route_message(msqid, topic, payload);
                    }
                }
            }
        }
    }
}