#include "tester.h"
#include "chat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

int tests_run = 0;
int tests_failed = 0;

// Импортируем глобальные переменные из chat.c для проверки
extern volatile sig_atomic_t g_exit_flag;
extern void sigint_handler(int sig);

// Вспомогательная функция для удаления старых очередей перед каждым тестом
static void force_unlink_queues() {
    mq_unlink("/test_unit_q_1");
    mq_unlink("/test_unit_q_2");
}

// Тест 1: Проверка форматирования имен очередей
static void test_queue_name_formatting() {
    chat_session_t session;
    
    // Случай без ведущего слэша
    format_queue_names("test_unit_q", &session);
    ASSERT_TRUE("Имя q1 должно начинаться со слэша", session.q1_name[0] == '/');
    ASSERT_TRUE("Имя q1 должно оканчиваться на _1", strcmp(session.q1_name, "/test_unit_q_1") == 0);
    ASSERT_TRUE("Имя q2 должно оканчиваться на _2", strcmp(session.q2_name, "/test_unit_q_2") == 0);

    // Случай с ведущим слэшем
    format_queue_names("/test_unit_q", &session);
    ASSERT_TRUE("Слэш не должен дублироваться", strcmp(session.q1_name, "/test_unit_q_1") == 0);
}

// Тест 2: Инициализация очередей в роли Создателя
static void test_creator_initialization() {
    force_unlink_queues();

    chat_session_t session;
    format_queue_names("/test_unit_q", &session);

    int res = init_chat_queues(&session);
    ASSERT_TRUE("init_chat_queues должен вернуть 0 при успехе", res == 0);
    ASSERT_TRUE("Процесс должен получить роль Создателя", session.is_creator == 1);
    ASSERT_TRUE(" rx_q дескриптор должен быть валидным", session.rx_q != (mqd_t)-1);
    ASSERT_TRUE(" tx_q дескриптор должен быть валидным", session.tx_q != (mqd_t)-1);

    cleanup_chat_queues(&session);
}

// Тест 3: Инициализация очередей в роли Присоединившегося (Follower)
static void test_follower_initialization() {
    force_unlink_queues();

    chat_session_t creator_sess;
    chat_session_t follower_sess;

    format_queue_names("/test_unit_q", &creator_sess);
    format_queue_names("/test_unit_q", &follower_sess);

    // Создатель открывает очереди первым
    int res1 = init_chat_queues(&creator_sess);
    ASSERT_TRUE("Создатель не смог инициализироваться", res1 == 0);

    // Присоединившийся пытается открыть те же очереди вторым
    int res2 = init_chat_queues(&follower_sess);
    ASSERT_TRUE("Присоединившийся не смог подключиться к готовым очередям", res2 == 0);
    ASSERT_TRUE("Присоединившийся не должен быть Создателем", follower_sess.is_creator == 0);
    
    // Проверяем зеркальность дескрипторов (проверить на отсутствие ошибок открытия)
    ASSERT_TRUE("rx_q Присоединившегося должен быть валидным", follower_sess.rx_q != (mqd_t)-1);
    ASSERT_TRUE("tx_q Присоединившегося должен быть валидным", follower_sess.tx_q != (mqd_t)-1);

    cleanup_chat_queues(&follower_sess);
    cleanup_chat_queues(&creator_sess);
}

// Тест 4: Передача данных с приоритетом выхода (255)
static void test_exit_priority_message() {
    force_unlink_queues();

    chat_session_t creator;
    chat_session_t follower;
    format_queue_names("/test_unit_q", &creator);
    format_queue_names("/test_unit_q", &follower);

    init_chat_queues(&creator);
    init_chat_queues(&follower);

    // Создатель отправляет тестовый прощальный пакет с приоритетом 255 в tx_q (очередь _2)
    const char *bye_msg = "EXIT";
    int send_res = mq_send(creator.tx_q, bye_msg, strlen(bye_msg), 255);
    ASSERT_TRUE("Не удалось отправить сообщение с приоритетом 255", send_res == 0);

    // Присоединившийся принимает сообщение из своей rx_q (очередь _2)
    char buf[256];
    unsigned int prio = 0;
    ssize_t bytes_read = mq_receive(follower.rx_q, buf, sizeof(buf), &prio);

    ASSERT_TRUE("Ошибка чтения сообщения", bytes_read >= 0);
    ASSERT_TRUE("Приоритет полученного сообщения должен быть равен 255", prio == 255);

    cleanup_chat_queues(&follower);
    cleanup_chat_queues(&creator);
}

// Тест 5: Граничный случай — Спам-атака (Превышение лимита очереди в 10 сообщений)
static void test_queue_spam_overflow() {
    force_unlink_queues();

    chat_session_t creator;
    format_queue_names("/test_unit_q", &creator);
    init_chat_queues(&creator);

    // По умолчанию очередь блокирует поток при переполнении.
    // Чтобы наш тестер не завис, мы временно переводим дескриптор отправки в неблокирующий режим (O_NONBLOCK).
    struct mq_attr new_attr, old_attr;
    new_attr.mq_flags = O_NONBLOCK;
    int attr_res = mq_setattr(creator.tx_q, &new_attr, &old_attr);
    ASSERT_TRUE("Не удалось установить неблокирующий режим для теста переполнения", attr_res == 0);

    // Спамим в очередь 10 сообщений (это её лимит MAX_MESSAGES)
    const char *spam_msg = "spam";
    for (int i = 0; i < 10; i++) {
        int res = mq_send(creator.tx_q, spam_msg, strlen(spam_msg), 1);
        ASSERT_TRUE("Очередь должна принимать первые 10 сообщений без проблем", res == 0);
    }

    // Пробуем отправить 11-е сообщение в заполненную очередь
    int over_res = mq_send(creator.tx_q, spam_msg, strlen(spam_msg), 1);
    
    // Так как включен O_NONBLOCK, отправка должна завершиться ошибкой, а errno показать EAGAIN
    ASSERT_TRUE("11-е сообщение должно вернуть ошибку (-1)", over_res == -1);
    ASSERT_TRUE("Код ошибки должен быть EAGAIN (очередь переполнена)", errno == EAGAIN);

    // Возвращаем атрибуты обратно перед очисткой (хороший тон программирования)
    mq_setattr(creator.tx_q, &old_attr, NULL);

    cleanup_chat_queues(&creator);
}

// Тест 6: Тестирование перехвата сигналов (SIGINT)
static void test_signal_handling() {
    // Настраиваем обработчик сигналов для теста
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    g_exit_flag = 0;

    // Имитируем отправку сигнала SIGINT (Ctrl+C) самому себе
    raise(SIGINT);

    // Проверяем, сработал ли обработчик и взвелся ли наш атомарный флаг выхода
    ASSERT_TRUE("Обработчик сигнала SIGINT должен взвести флаг выхода g_exit_flag в 1", g_exit_flag == 1);
}

int main() {
    printf("Запуск автоматических модульных тестов: ");
    fflush(stdout);

    RUN_TEST(test_queue_name_formatting);
    RUN_TEST(test_creator_initialization);
    RUN_TEST(test_follower_initialization);
    RUN_TEST(test_exit_priority_message);
    RUN_TEST(test_queue_spam_overflow);
    RUN_TEST(test_signal_handling);

    // Удаляем временные очереди после завершения всех тестов
    force_unlink_queues();

    printf("\nЗапущено тестов: %d, Ошибок: %d\n", tests_run, tests_failed);

    if (tests_failed > 0) {
        printf("[РЕЗУЛЬТАТ]: Тесты провалены!\n");
        return EXIT_FAILURE;
    }
    printf("[РЕЗУЛЬТАТ]: Все тесты успешно пройдены!\n");
    return EXIT_SUCCESS;
}