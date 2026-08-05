#include "broker.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

int tests_run = 0;
int tests_failed = 0;

// Тестовые макросы
#define RUN_TEST(test_func) do { \
    printf("Запуск %s... ", #test_func); \
    fflush(stdout); \
    tests_run++; \
    test_func(); \
    printf("Успешно!\n"); \
} while (0)

#define ASSERT_TRUE(message, condition) do { \
    if (!(condition)) { \
        printf("\n[ОШИБКА] %s:%d: %s\n", __FILE__, __LINE__, message); \
        tests_failed++; \
        return; \
    } \
} while (0)

// Очистка очереди перед каждым тестом
static void force_cleanup_queue() {
    key_t key = ftok(QUEUE_PATH, QUEUE_PROJECT_ID);
    if (key != -1) {
        int msqid = msgget(key, 0666);
        if (msqid != -1) {
            msgctl(msqid, IPC_RMID, NULL);
        }
    }
}

static pid_t broker_pid = -1;
static int global_msqid = -1;

// Вспомогательная функция для запуска брокера в фоне
static void start_broker_child() {
    force_cleanup_queue();
    broker_pid = fork();
    if (broker_pid == 0) {
        // Дочерний процесс: запускаем реального брокера
        run_broker();
        exit(0);
    }
    // Родительский процесс: ждем инициализации очереди брокером
    usleep(100000); // 100мс
    key_t key = ftok(QUEUE_PATH, QUEUE_PROJECT_ID);
    global_msqid = msgget(key, 0666);
}

// Вспомогательная функция для остановки фонового брокера
static void stop_broker_child() {
    if (broker_pid > 0) {
        kill(broker_pid, SIGINT);
        waitpid(broker_pid, NULL, 0);
        broker_pid = -1;
    }
    force_cleanup_queue();
    global_msqid = -1;
}

// Тест 1: Попытка подключения к несуществующей очереди брокера
static void test_connect_no_broker() {
    force_cleanup_queue();
    key_t key = ftok(QUEUE_PATH, QUEUE_PROJECT_ID);
    ASSERT_TRUE("Ключ ftok должен быть валидным", key != -1);

    int msqid = msgget(key, 0666);
    ASSERT_TRUE("Подключение к несуществующей очереди должно вернуть -1", msqid == -1);
    ASSERT_TRUE("Код ошибки должен быть ENOENT", errno == ENOENT);
}

// Тест 2: Проверка монопольного запуска брокера (IPC_EXCL)
static void test_duplicate_broker() {
    force_cleanup_queue();
    key_t key = ftok(QUEUE_PATH, QUEUE_PROJECT_ID);
    
    int msqid1 = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    ASSERT_TRUE("Первый запуск брокера должен быть успешным", msqid1 != -1);

    int msqid2 = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    ASSERT_TRUE("Запуск дубликата брокера должен вернуть -1", msqid2 == -1);
    ASSERT_TRUE("Код ошибки дубликата должен быть EEXIST", errno == EEXIST);

    msgctl(msqid1, IPC_RMID, NULL);
}

// Тест 3: Дефолтная маршрутизация на разные темы для разных подписчиков
static void test_default_routing() {
    start_broker_child();
    ASSERT_TRUE("Брокер должен успешно создать очередь", global_msqid != -1);

    // Подписываем Sub1 (PID 10001) на "news"
    struct msgbuf sub1;
    sub1.mtype = 1;
    strcpy(sub1.mtext, "subscribe,10001,news");
    int res = msgsnd(global_msqid, &sub1, strlen(sub1.mtext) + 1, 0);
    ASSERT_TRUE("Отправка подписки Sub1", res == 0);

    // Подписываем Sub2 (PID 10002) на "sport"
    struct msgbuf sub2;
    sub2.mtype = 1;
    strcpy(sub2.mtext, "subscribe,10002,sport");
    res = msgsnd(global_msqid, &sub2, strlen(sub2.mtext) + 1, 0);
    ASSERT_TRUE("Отправка подписки Sub2", res == 0);

    usleep(50000); // Даем время брокеру обработать подписки

    // Публикация в "news"
    struct msgbuf pub1;
    pub1.mtype = 1;
    strcpy(pub1.mtext, "send,20001,news,Hello News");
    res = msgsnd(global_msqid, &pub1, strlen(pub1.mtext) + 1, 0);
    ASSERT_TRUE("Публикация в news", res == 0);

    // Публикация в "sport"
    struct msgbuf pub2;
    pub2.mtype = 1;
    strcpy(pub2.mtext, "send,20002,sport,Hello Sport");
    res = msgsnd(global_msqid, &pub2, strlen(pub2.mtext) + 1, 0);
    ASSERT_TRUE("Публикация в sport", res == 0);

    usleep(50000); // Ожидаем маршрутизацию

    // Проверяем получение Sub1 (PID 10001)
    struct msgbuf rx1;
    ssize_t rx_res = msgrcv(global_msqid, &rx1, sizeof(rx1.mtext), 10001, IPC_NOWAIT);
    ASSERT_TRUE("Sub1 должен получить свое сообщение", rx_res != -1);
    ASSERT_TRUE("Содержимое сообщения Sub1", strcmp(rx1.mtext, "[news]: Hello News") == 0);

    // Проверяем получение Sub2 (PID 10002)
    struct msgbuf rx2;
    rx_res = msgrcv(global_msqid, &rx2, sizeof(rx2.mtext), 10002, IPC_NOWAIT);
    ASSERT_TRUE("Sub2 должен получить свое сообщение", rx_res != -1);
    ASSERT_TRUE("Содержимое сообщения Sub2", strcmp(rx2.mtext, "[sport]: Hello Sport") == 0);

    // Проверяем изоляцию (Sub1 не должен получить сообщение из sport)
    rx_res = msgrcv(global_msqid, &rx1, sizeof(rx1.mtext), 10001, IPC_NOWAIT);
    ASSERT_TRUE("У Sub1 не должно быть лишних сообщений", rx_res == -1 && errno == ENOMSG);

    stop_broker_child();
}

// Тест 4: Один подписчик на несколько тем одновременно (3+ темы)
static void test_one_sub_multiple_topics() {
    start_broker_child();

    // Один подписчик (PID 10003) подписывается на "t1", "t2", "t3"
    struct msgbuf sub;
    sub.mtype = 1;
    
    strcpy(sub.mtext, "subscribe,10003,t1");
    msgsnd(global_msqid, &sub, strlen(sub.mtext) + 1, 0);
    strcpy(sub.mtext, "subscribe,10003,t2");
    msgsnd(global_msqid, &sub, strlen(sub.mtext) + 1, 0);
    strcpy(sub.mtext, "subscribe,10003,t3");
    msgsnd(global_msqid, &sub, strlen(sub.mtext) + 1, 0);

    usleep(50000);

    // Публикация сообщений во все три темы
    struct msgbuf pub;
    pub.mtype = 1;
    
    strcpy(pub.mtext, "send,20001,t1,msg1");
    msgsnd(global_msqid, &pub, strlen(pub.mtext) + 1, 0);
    strcpy(pub.mtext, "send,20001,t2,msg2");
    msgsnd(global_msqid, &pub, strlen(pub.mtext) + 1, 0);
    strcpy(pub.mtext, "send,20001,t3,msg3");
    msgsnd(global_msqid, &pub, strlen(pub.mtext) + 1, 0);

    usleep(50000);

    // Подписчик 10003 должен получить все три сообщения последовательно
    struct msgbuf rx;
    ssize_t res;
    
    res = msgrcv(global_msqid, &rx, sizeof(rx.mtext), 10003, IPC_NOWAIT);
    ASSERT_TRUE("Сообщение 1 получено", res != -1 && strcmp(rx.mtext, "[t1]: msg1") == 0);

    res = msgrcv(global_msqid, &rx, sizeof(rx.mtext), 10003, IPC_NOWAIT);
    ASSERT_TRUE("Сообщение 2 получено", res != -1 && strcmp(rx.mtext, "[t2]: msg2") == 0);

    res = msgrcv(global_msqid, &rx, sizeof(rx.mtext), 10003, IPC_NOWAIT);
    ASSERT_TRUE("Сообщение 3 получено", res != -1 && strcmp(rx.mtext, "[t3]: msg3") == 0);

    stop_broker_child();
}

// Тест 5: На одну тему подписано несколько подписчиков одновременно (3+ подписчика)
static void test_one_topic_multiple_subs() {
    start_broker_child();

    // Подписчики 10004, 10005, 10006 подписываются на "tech"
    struct msgbuf sub;
    sub.mtype = 1;

    strcpy(sub.mtext, "subscribe,10004,tech");
    msgsnd(global_msqid, &sub, strlen(sub.mtext) + 1, 0);
    strcpy(sub.mtext, "subscribe,10005,tech");
    msgsnd(global_msqid, &sub, strlen(sub.mtext) + 1, 0);
    strcpy(sub.mtext, "subscribe,10006,tech");
    msgsnd(global_msqid, &sub, strlen(sub.mtext) + 1, 0);

    usleep(50000);

    // Публикуем сообщение в тему "tech"
    struct msgbuf pub;
    pub.mtype = 1;
    strcpy(pub.mtext, "send,20002,tech,update");
    msgsnd(global_msqid, &pub, strlen(pub.mtext) + 1, 0);

    usleep(50000);

    // Проверяем, что все три подписчика получили копию сообщения
    struct msgbuf rx;
    ssize_t res;

    res = msgrcv(global_msqid, &rx, sizeof(rx.mtext), 10004, IPC_NOWAIT);
    ASSERT_TRUE("Подписчик 1 получено", res != -1 && strcmp(rx.mtext, "[tech]: update") == 0);

    res = msgrcv(global_msqid, &rx, sizeof(rx.mtext), 10005, IPC_NOWAIT);
    ASSERT_TRUE("Подписчик 2 получено", res != -1 && strcmp(rx.mtext, "[tech]: update") == 0);

    res = msgrcv(global_msqid, &rx, sizeof(rx.mtext), 10006, IPC_NOWAIT);
    ASSERT_TRUE("Подписчик 3 получено", res != -1 && strcmp(rx.mtext, "[tech]: update") == 0);

    stop_broker_child();
}

// Тест 6: Граничные значения и Логика отписки (выход подписчика первым)
static void test_boundary_and_unsubscribe() {
    start_broker_child();

    // Граничное значение: очень длинная тема (100 символов, превосходящих буфер в 64 байта)
    char long_topic[120];
    memset(long_topic, 'A', 100);
    long_topic[100] = '\0';

    struct msgbuf sub;
    sub.mtype = 1;
    snprintf(sub.mtext, sizeof(sub.mtext), "subscribe,10007,%s", long_topic);
    msgsnd(global_msqid, &sub, strlen(sub.mtext) + 1, 0);

    usleep(50000);

    // Подписчик 10007 решает отписаться перед выходом
    struct msgbuf unsub;
    unsub.mtype = 1;
    snprintf(unsub.mtext, sizeof(unsub.mtext), "unsubscribe,10007,%s", long_topic);
    msgsnd(global_msqid, &unsub, strlen(unsub.mtext) + 1, 0);

    usleep(50000);

    // Публикуем сообщение в эту тему
    struct msgbuf pub;
    pub.mtype = 1;
    snprintf(pub.mtext, sizeof(pub.mtext), "send,20003,%s,test_payload", long_topic);
    msgsnd(global_msqid, &pub, strlen(pub.mtext) + 1, 0);

    usleep(50000);

    // Так как подписчик отписался, он не должен ничего получить
    struct msgbuf rx;
    ssize_t res = msgrcv(global_msqid, &rx, sizeof(rx.mtext), 10007, IPC_NOWAIT);
    ASSERT_TRUE("После отписки подписчик не должен получать сообщения темы", res == -1 && errno == ENOMSG);

    stop_broker_child();
}

// Тест 7: Выход брокера первым (проверка EIDRM / EINVAL для клиентов)
static void test_broker_exits_first() {
    start_broker_child();
    int saved_msqid = global_msqid;
    ASSERT_TRUE("Очередь должна быть создана", saved_msqid != -1);

    // Брокер завершает работу первым
    stop_broker_child();

    // Клиент пытается отправить сообщение в удаленную очередь
    struct msgbuf msg;
    msg.mtype = 1;
    strcpy(msg.mtext, "test");
    int res = msgsnd(saved_msqid, &msg, strlen(msg.mtext) + 1, 0);
    
    ASSERT_TRUE("Отправка в удаленную брокером очередь должна завершиться ошибкой", res == -1);
    ASSERT_TRUE("Ошибка должна быть EIDRM или EINVAL", errno == EIDRM || errno == EINVAL);
}

// Тест 8: Конкуренция и спам (3 параллельных издателя спамят по 20 сообщений одновременно)
static void test_concurrency_and_spam() {
    start_broker_child();
    ASSERT_TRUE("Брокер должен успешно создать очередь", global_msqid != -1);

    // Подписываем Sub (PID 10008) на "spam_topic"
    struct msgbuf sub;
    sub.mtype = 1;
    strcpy(sub.mtext, "subscribe,10008,spam_topic");
    int res = msgsnd(global_msqid, &sub, strlen(sub.mtext) + 1, 0);
    ASSERT_TRUE("Подписка на spam_topic", res == 0);

    usleep(50000); // Ожидаем подписку

    // Запускаем 3 параллельных процесса-издателя
    pid_t pub_pids[3];
    for (int i = 0; i < 3; i++) {
        pub_pids[i] = fork();
        if (pub_pids[i] == 0) {
            // Код дочернего процесса (Издателя)
            for (int j = 0; j < 20; j++) {
                struct msgbuf pub;
                pub.mtype = 1;
                // Формат сообщения: send,PID,spam_topic,msg_<номер_издателя>_<номер_сообщения>
                snprintf(pub.mtext, sizeof(pub.mtext), "send,%d,spam_topic,msg_%d_%d", getpid(), i, j);
                
                // Используем IPC_NOWAIT на случай, если очередь переполнится
                if (msgsnd(global_msqid, &pub, strlen(pub.mtext) + 1, IPC_NOWAIT) == -1) {
                    exit(EXIT_FAILURE);
                }
                usleep(1000); // Небольшая задержка между отправками
            }
            exit(EXIT_SUCCESS);
        }
    }

    // Ждем завершения всех трех издателей
    for (int i = 0; i < 3; i++) {
        int status;
        waitpid(pub_pids[i], &status, 0);
        ASSERT_TRUE("Каждый издатель должен завершить отправку без сбоев", WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }

    // Даем брокеру время разложить все сообщения по полочкам
    usleep(150000);

    // Считываем сообщения из ящика подписчика 10008
    int received_count = 0;
    struct msgbuf rx;
    while (1) {
        ssize_t rx_res = msgrcv(global_msqid, &rx, sizeof(rx.mtext), 10008, IPC_NOWAIT);
        if (rx_res == -1) {
            if (errno == ENOMSG) {
                break; // Сообщения закончились
            }
            perror("[Тест] Ошибка msgrcv");
            break;
        }
        received_count++;
    }

    // Ожидаем ровно 60 доставленных сообщений (3 издателя * 20 сообщений)
    ASSERT_TRUE("Должно быть успешно доставлено ровно 60 сообщений", received_count == 60);

    stop_broker_child();
}

int main() {
    printf("Запуск автоматических тестов System V очереди сообщений:\n");
    fflush(stdout);

    RUN_TEST(test_connect_no_broker);
    RUN_TEST(test_duplicate_broker);
    RUN_TEST(test_default_routing);
    RUN_TEST(test_one_sub_multiple_topics);
    RUN_TEST(test_one_topic_multiple_subs);
    RUN_TEST(test_boundary_and_unsubscribe);
    RUN_TEST(test_broker_exits_first);
    RUN_TEST(test_concurrency_and_spam); 

    printf("\nЗапущено тестов: %d, Ошибок: %d\n", tests_run, tests_failed);

    if (tests_failed > 0) {
        printf("[РЕЗУЛЬТАТ]: Тесты провалены!\n");
        return EXIT_FAILURE;
    }
    printf("[РЕЗУЛЬТАТ]: Все тесты успешно пройдены!\n");
    return EXIT_SUCCESS;
}