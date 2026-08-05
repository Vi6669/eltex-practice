#include "chat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

// Глобальные переменные для безопасного обмена статусом с обработчиками сигналов
volatile sig_atomic_t g_exit_flag = 0;
pthread_t main_thread_id;

// Обработчик Ctrl+C
void sigint_handler(int sig) {
    (void)sig;
    g_exit_flag = 1; 
}

// Пустой обработчик для прерывания блокирующего fgets()
void sigusr1_handler(int sig) {
    (void)sig; 
}

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
    session->peer_exited = 0;

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

void *receive_thread_func(void *arg) {
    chat_session_t *session = (chat_session_t *)arg;
    char rx_buffer[MSG_BUFFER_SIZE];
    unsigned int prio;

    while (1) {
        ssize_t bytes_read = mq_receive(session->rx_q, rx_buffer, MSG_BUFFER_SIZE, &prio);
        
        if (bytes_read >= 0) {
            rx_buffer[bytes_read] = '\0';
            
            // Если пришло сообщение с высоким приоритетом 255 — собеседник вышел
            if (prio == EXIT_PRIORITY) {
                printf("\r[Система]: Собеседник закрыл чат. Нажмите Enter для выхода...\n");
                session->peer_exited = 1;
                g_exit_flag = 1;
                
                // Прерываем fgets() основного потока, отправляя ему SIGUSR1
                pthread_kill(main_thread_id, SIGUSR1);
                break;
            }

            printf("\r[Собеседник]: %s\n> ", rx_buffer);
            fflush(stdout);
        } else {
            break;
        }
    }
    return NULL;
}

void run_chat_loop(chat_session_t *session) {
    char tx_buffer[MSG_BUFFER_SIZE];
    main_thread_id = pthread_self(); // Запоминаем ID основного потока

    // Настройка обработки SIGINT (Ctrl+C)
    struct sigaction sa_int;
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0; // Не устанавливаем SA_RESTART, чтобы прерывать fgets()
    sigaction(SIGINT, &sa_int, NULL);

    // Настройка обработки SIGUSR1 (внутреннее пробуждение потока)
    struct sigaction sa_usr;
    sa_usr.sa_handler = sigusr1_handler;
    sigemptyset(&sa_usr.sa_mask);
    sa_usr.sa_flags = 0;
    sigaction(SIGUSR1, &sa_usr, NULL);

    // Запуск фонового потока приема
    if (pthread_create(&session->rx_thread, NULL, receive_thread_func, session) != 0) {
        perror("Ошибка запуска приемного потока");
        return;
    }

    printf("\n==== ЧАТ ЗАПУЩЕН ====\n");
    printf("Выход: '/exit', Ctrl+D или Ctrl+C.\n\n");
    printf("> ");
    fflush(stdout);

    while (!g_exit_flag) {
        if (fgets(tx_buffer, sizeof(tx_buffer), stdin) == NULL) {
            // Проверяем: был ли fgets прерван сигналом (SIGINT или SIGUSR1)
            if (g_exit_flag) {
                break;
            }
            // Если fgets вернул NULL без флага — это Ctrl+D (EOF)
            g_exit_flag = 1;
            break;
        }

        size_t len = strlen(tx_buffer);
        if (len > 0 && tx_buffer[len - 1] == '\n') {
            tx_buffer[len - 1] = '\0';
        }

        if (strcmp(tx_buffer, "/exit") == 0) {
            g_exit_flag = 1;
            break;
        }

        if (strlen(tx_buffer) == 0) {
            printf("> ");
            fflush(stdout);
            continue;
        }

        // Обычные сообщения отправляем с приоритетом 1
        if (mq_send(session->tx_q, tx_buffer, strlen(tx_buffer), 1) < 0) {
            perror("\nОшибка отправки сообщения");
        }

        printf("> ");
        fflush(stdout);
    }

    // Если мы выходим по своей инициативе 
    if (g_exit_flag && !session->peer_exited) {
        // Отправляем специальный сигнал выхода с приоритетом 255
        mq_send(session->tx_q, "EXIT", 4, EXIT_PRIORITY);
    }

    printf("\nЗавершение сессии чата...\n");

    // Останавливаем фоновый поток
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