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
        // Мы успешно создали первую очередь -> Роль: Создатель
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
        // Очередь уже создана -> Роль: Присоединившийся
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