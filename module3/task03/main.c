#include <stdio.h>
#include <stdlib.h>
#include "chat.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Использование: %s <имя_очереди>\n", argv[0]);
        fprintf(stderr, "Пример: %s /my_chat\n", argv[0]);
        return EXIT_FAILURE;
    }

    chat_session_t session;

    format_queue_names(argv[1], &session);

    printf("Попытка инициализации сессии для: %s\n", argv[1]);

    if (init_chat_queues(&session) < 0) {
        fprintf(stderr, "Критическая ошибка при инициализации очередей.\n");
        return EXIT_FAILURE;
    }

    run_chat_loop(&session);

    cleanup_chat_queues(&session);

    printf("Программа завершена.\n");
    return EXIT_SUCCESS;
}