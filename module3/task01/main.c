#include <stdio.h>
#include <stdlib.h>
#include "copier.h"

int main(int argc, char *argv[]) {
    // Ожидаем как минимум один исходный файл в качестве аргумента
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> [file2] [file3] ...\n", argv[0]);
        return EXIT_FAILURE;
    }

    int overall_status = EXIT_SUCCESS;

    // Последовательно обрабатываем каждый файл
    for (int i = 1; i < argc; i++) {
        // На этом шаге передаем NULL вместо имени FIFO-канала
        if (copy_file(argv[i], NULL) != 0) {
            overall_status = EXIT_FAILURE;
        }
    }

    return overall_status;
}