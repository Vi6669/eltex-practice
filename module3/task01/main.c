#include <stdio.h>
#include <stdlib.h>
#include "copier.h"

int main(int argc, char *argv[]) {
    // Проверяем, передан ли хотя бы один исходный файл
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> [file2] [file3] ...\n", argv[0]);
        return EXIT_FAILURE;
    }

    int overall_status = EXIT_SUCCESS;

    // Последовательно обрабатываем каждый файл, переданный в аргументах
    for (int i = 1; i < argc; i++) {
        if (copy_file_single_process(argv[i]) != 0) {
            overall_status = EXIT_FAILURE;
        }
    }

    return overall_status;
}