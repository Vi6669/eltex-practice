#include "chat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

void format_queue_names(const char *base_name, chat_session_t *session) {
    if (base_name[0] == '/') {
        snprintf(session->q1_name, sizeof(session->q1_name), "%s_1", base_name);
        snprintf(session->q2_name, sizeof(session->q2_name), "%s_2", base_name);
    } else {
        snprintf(session->q1_name, sizeof(session->q1_name), "/%s_1", base_name);
        snprintf(session->q2_name, sizeof(session->q2_name), "/%s_2", base_name);
    }
}

int init_chat_queues(chat_session_t *session) {
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = MSG_BUFFER_SIZE;
    attr.mq_curmsgs = 0;

    session->rx_q = (mqd_t)-1;
    session->tx_q = (mqd_t)-1;
    session->is_creator = 0;

    // Попытка создания первой очереди монопольно
    session->rx_q = mq_open(session->q1_name, O_RDONLY | O_CREAT | O_EXCL, 0660, &attr);

    if (session->rx_q != (mqd_t)-1) {
        session->is_creator = 1;
        printf("[Создатель] Очередь %s успешно создана.\n", session->q1_name);

        session->tx_q = mq_open(session->q2_name, O_WRONLY | O_CREAT | O_EXCL, 0660, &attr);
        if (session->tx_q == (mqd_t)-1) {
            perror("[Создатель] Ошибка создания второй очереди");
            mq_close(session->rx_q);
            mq_unlink(session->q1_name);
            return -1;
        }
        printf("[Создатель] Очередь %s успешно создана.\n", session->q2_name);
        printf("[Создатель] Режим: Прием из _1, Отправка в _2.\n");
    } else {
        if (errno == EEXIST) {
            printf("[Присоединившийся] Обнаружены существующие очереди. Подключение...\n");

            session->tx_q = mq_open(session->q1_name, O_WRONLY);
            if (session->tx_q == (mqd_t)-1) {
                perror("[Присоединившийся] Ошибка открытия очереди _1 на запись");
                return -1;
            }

            session->rx_q = mq_open(session->q2_name, O_RDONLY);
            if (session->rx_q == (mqd_t)-1) {
                perror("[Присоединившийся] Ошибка открытия очереди _2 на чтение");
                mq_close(session->tx_q);
                return -1;
            }
            printf("[Присоединившийся] Режим: Отправка в _1, Прием из _2.\n");
        } else {
            perror("Не удалось открыть/создать очереди");
            return -1;
        }
    }
    return 0;
}

// Фоновый поток для непрерывного чтения входящих сообщений
void *receive_thread_func(void *arg) {
    chat_session_t *session = (chat_session_t *)arg;
    char rx_buffer[MSG_BUFFER_SIZE];
    unsigned int prio;

    while (1) {
        // Системный вызов заблокирует поток, пока в очереди не появится сообщение
        ssize_t bytes_read = mq_receive(session->rx_q, rx_buffer, MSG_BUFFER_SIZE, &prio);
        
        if (bytes_read >= 0) {
            rx_buffer[bytes_read] = '\0'; // Гарантируем корректное завершение строки
            
            // Выводим сообщение собеседника. Символы "\r" и "> " помогают 
            // не портить внешний вид строки ввода при получении сообщения.
            printf("\r[Собеседник]: %s\n> ", rx_buffer);
            fflush(stdout);
        } else {
            // Если вызов mq_receive вернул ошибку (например, очередь закрыли), завершаем поток
            break;
        }
    }
    return NULL;
}

// Главный цикл чата (выполняется в основном потоке программы)
void run_chat_loop(chat_session_t *session) {
    char tx_buffer[MSG_BUFFER_SIZE];

    // 1. Запускаем фоновый поток для приема входящих сообщений
    if (pthread_create(&session->rx_thread, NULL, receive_thread_func, session) != 0) {
        perror("Ошибка запуска приемного потока");
        return;
    }

    printf("\n==== ЧАТ ЗАПУЩЕН ====\n");
    printf("Введите сообщение и нажмите Enter. Для выхода наберите '/exit' или нажмите Ctrl+D.\n\n");
    printf("> ");
    fflush(stdout);

    // 2. Основной поток читает ввод пользователя и отправляет сообщения
    while (1) {
        if (fgets(tx_buffer, sizeof(tx_buffer), stdin) == NULL) {
            // Пользователь нажал Ctrl+D (EOF)
            break;
        }

        // Удаляем символ переноса строки из конца ввода
        size_t len = strlen(tx_buffer);
        if (len > 0 && tx_buffer[len - 1] == '\n') {
            tx_buffer[len - 1] = '\0';
        }

        // Проверяем команду выхода
        if (strcmp(tx_buffer, "/exit") == 0) {
            break;
        }

        // Игнорируем пустые сообщения
        if (strlen(tx_buffer) == 0) {
            printf("> ");
            fflush(stdout);
            continue;
        }

        // Отправляем сообщение с дефолтным приоритетом 1
        if (mq_send(session->tx_q, tx_buffer, strlen(tx_buffer), 1) < 0) {
            perror("\nОшибка отправки сообщения");
        }

        printf("> ");
        fflush(stdout);
    }

    printf("\nЗавершение сессии чата...\n");

    // 3. Останавливаем фоновый поток приема перед выходом
    pthread_cancel(session->rx_thread);
    pthread_join(session->rx_thread, NULL);
}

void cleanup_chat_queues(chat_session_t *session) {
    if (session->rx_q != (mqd_t)-1) {
        mq_close(session->rx_q);
        session->rx_q = (mqd_t)-1;
    }
    if (session->tx_q != (mqd_t)-1) {
        mq_close(session->tx_q);
        session->tx_q = (mqd_t)-1;
    }

    if (session->is_creator) {
        printf("[Создатель] Удаление очередей из системы...\n");
        mq_unlink(session->q1_name);
        mq_unlink(session->q2_name);
    }
}