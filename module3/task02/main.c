#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "broker.h"
#include "publisher.h"
#include "subscriber.h"

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Использование:\n");
    fprintf(stderr, "  Брокер:     %s -b\n", prog_name);
    fprintf(stderr, "  Издатель:  %s -p <тема>\n", prog_name);
    fprintf(stderr, "  Подписчик: %s -s <тема1> <тема2> ...\n", prog_name);
}

int main(int argc, char *argv[]) {
    int opt;
    int mode_b = 0, mode_p = 0, mode_s = 0;

    optind = 1; // Сброс для getopt

    while ((opt = getopt(argc, argv, "bps")) != -1) {
        switch (opt) {
            case 'b': mode_b = 1; break;
            case 'p': mode_p = 1; break;
            case 's': mode_s = 1; break;
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    int selected_modes = mode_b + mode_p + mode_s;
    if (selected_modes != 1) {
        fprintf(stderr, "Ошибка: Необходимо выбрать ровно один режим работы (-b, -p или -s).\n");
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if (mode_b) {
        run_broker();
    } else if (mode_p) {
        if (optind >= argc) {
            fprintf(stderr, "Ошибка: Режим издателя (-p) требует указания темы.\n");
            exit(EXIT_FAILURE);
        }
        run_publisher(argv[optind]);
    } else if (mode_s) {
        if (optind >= argc) {
            fprintf(stderr, "Ошибка: Режим подписчика (-s) требует указания хотя бы одной темы.\n");
            exit(EXIT_FAILURE);
        }
        run_subscriber(argc - optind, &argv[optind]);
    }

    return 0;
}               