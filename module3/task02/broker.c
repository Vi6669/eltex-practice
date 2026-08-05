#include "broker.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#define MAX_SUBSCRIBERS 128
#define MAX_PUBLISHERS 128
#define INACTIVITY_TIMEOUT 60 // Таймаут неактивности брокера в секундах

typedef struct {
    pid_t pid;
    char topic[64];
} Subscriber;

static Subscriber subscribers[MAX_SUBSCRIBERS];
static int subscriber_count = 0;

static pid_t publishers[MAX_PUBLISHERS];
static int publisher_count = 0;

static volatile sig_atomic_t shutdown_flag = 0;
static volatile sig_atomic_t timeout_flag = 0;

// Обработчик сигналов для брокера
static void broker_signal_handler(int sig) {
    if (sig == SIGINT) {
        shutdown_flag = 1;
    } else if (sig == SIGALRM) {
        timeout_flag = 1;
    }
}

// Настройка обработки сигналов SIGINT и SIGALRM
static void setup_signals(void) {
    struct sigaction sa;
    sa.sa_handler = broker_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Не устанавливаем SA_RESTART для прерывания msgrcv
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);
}

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

// Регистрация подписчика
static void register_subscriber(pid_t pid, const char *topic) {
    if (subscriber_count >= MAX_SUBSCRIBERS) {
        fprintf(stderr, "[Брокер] Превышен лимит подписчиков!\n");
        return;
    }
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

// Удаление подписки (отписка)
static void unregister_subscriber(pid_t pid, const char *topic) {
    for (int i = 0; i < subscriber_count; i++) {
        if (subscribers[i].pid == pid && strcmp(subscribers[i].topic, topic) == 0) {
            for (int j = i; j < subscriber_count - 1; j++) {
                subscribers[j] = subscribers[j + 1];
            }
            subscriber_count--;
            printf("[Брокер] Подписчик %d успешно отписан от темы '%s'\n", pid, topic);
            return;
        }
    }
}

// Регистрация издателя
static void register_publisher(pid_t pid) {
    if (publisher_count >= MAX_PUBLISHERS) {
        return;
    }
    for (int i = 0; i < publisher_count; i++) {
        if (publishers[i] == pid) {
            return; 
        }
    }
    publishers[publisher_count] = pid;
    publisher_count++;
    printf("[Брокер] Издатель %d добавлен в список активных отправителей\n", pid);
}

// Маршрутизация сообщения
static void route_message(int msqid, const char *topic, const char *payload) {
    struct msgbuf out_msg;
    snprintf(out_msg.mtext, sizeof(out_msg.mtext), "[%s]: %s", topic, payload);

    int routed_count = 0;
    for (int i = 0; i < subscriber_count; i++) {
        if (strcmp(subscribers[i].topic, topic) == 0) {
            out_msg.mtype = subscribers[i].pid; 
            
            if (msgsnd(msqid, &out_msg, strlen(out_msg.mtext) + 1, 0) == -1) {
                if (errno != EIDRM && errno != EINVAL) {
                    perror("[Брокер] Ошибка отправки подписчику");
                }
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

// Оповещение всех клиентов перед закрытием
static void notify_clients(void) {
    printf("[Брокер] Отправка сигнала SIGINT всем активным клиентам...\n");
    for (int i = 0; i < subscriber_count; i++) {
        kill(subscribers[i].pid, SIGINT);
    }
    for (int i = 0; i < publisher_count; i++) {
        kill(publishers[i], SIGINT);
    }
}

// Обработка подписки
static void handle_subscribe(const char *msg_text) {
    pid_t sub_pid;
    char topic[64];
    if (sscanf(msg_text, "subscribe,%d,%63s", &sub_pid, topic) == 2) {
        register_subscriber(sub_pid, topic);
    }
}

// Обработка отписки
static void handle_unsubscribe(const char *msg_text) {
    pid_t unsub_pid;
    char topic[64];
    if (sscanf(msg_text, "unsubscribe,%d,%63s", &unsub_pid, topic) == 2) {
        unregister_subscriber(unsub_pid, topic);
    }
}

// Обработка и маршрутизация публикации
static void handle_send(int msqid, const char *msg_text) {
    pid_t pub_pid;
    char topic[64];
    
    char *comma1 = strchr(msg_text, ','); 
    if (!comma1) return;
    char *comma2 = strchr(comma1 + 1, ','); 
    if (!comma2) return;
    char *comma3 = strchr(comma2 + 1, ','); 
    if (!comma3) return;

    pub_pid = atoi(comma1 + 1);
    
    int topic_len = comma3 - (comma2 + 1);
    if (topic_len > 63) topic_len = 63;
    strncpy(topic, comma2 + 1, topic_len);
    topic[topic_len] = '\0';
    
    const char *payload = comma3 + 1;
    
    register_publisher(pub_pid);
    route_message(msqid, topic, payload);
}

// Распределитель входящих сообщений
static void process_incoming_message(int msqid, const char *msg_text) {
    if (strncmp(msg_text, "subscribe,", 10) == 0) {
        handle_subscribe(msg_text);
    } else if (strncmp(msg_text, "unsubscribe,", 12) == 0) {
        handle_unsubscribe(msg_text);
    } else if (strncmp(msg_text, "send,", 5) == 0) {
        handle_send(msqid, msg_text);
    }
}

// Завершение работы брокера и очистка очереди
static void shutdown_broker(int msqid) {
    if (timeout_flag) {
        printf("\n[Брокер] Завершение работы по таймауту неактивности (%d сек).\n", INACTIVITY_TIMEOUT);
    } else if (shutdown_flag) {
        printf("\n[Брокер] Получен сигнал завершения SIGINT.\n");
    }

    notify_clients();

    if (msgctl(msqid, IPC_RMID, NULL) == -1) {
        perror("[Брокер] Ошибка удаления очереди сообщений");
    } else {
        printf("[Брокер] Очередь сообщений (ID: %d) успешно удалена из системы.\n", msqid);
    }
}

void run_broker(void) {
    printf("[Брокер] Запуск...\n");
    
    int msqid = create_exclusive_queue();
    printf("[Брокер] Очередь сообщений успешно создана (ID: %d).\n", msqid);

    setup_signals();

    struct msgbuf buf;
    alarm(INACTIVITY_TIMEOUT);

    while (!shutdown_flag && !timeout_flag) {
        if (msgrcv(msqid, &buf, sizeof(buf.mtext), 1, 0) == -1) {
            if (errno == EINTR) {
                // Системный вызов был прерван сигналом SIGINT или SIGALRM
                break;
            }
            perror("[Брокер] Ошибка msgrcv");
            break;
        }

        alarm(INACTIVITY_TIMEOUT);
        process_incoming_message(msqid, buf.mtext);
    }

    shutdown_broker(msqid);
}