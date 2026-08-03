#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "copier.h"

int main(int argc, char *argv[]) {
    const char *fifo_name = NULL;

    // 1. Ручной разбор аргументов для поиска ключа -p и его значения
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 < argc) {
                fifo_name = argv[i + 1];
                
                // Чтобы убрать "-p" и "<имя_канала>" из списка файлов для копирования
                for (int j = i; j < argc - 2; j++) {
                    argv[j] = argv[j + 2];
                }
                argc -= 2; // Уменьшаем счетчик аргументов на 2
                i--;       // Перепроверяем текущий индекс на новой итерации
            } else {
                fprintf(stderr, "Error: Missing argument for option -p\n");
                return EXIT_FAILURE;
            }
        }
    }

    // 2. Если после разбора ключей не осталось файлов для копирования, выводим инструкцию
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [-p <fifo_name>] <file1> [file2] [file3] ...\n", argv[0]);
        return EXIT_FAILURE;
    }

    int overall_status = EXIT_SUCCESS;

    // 3. Последовательно копируем каждый указанный файл
    for (int i = 1; i < argc; i++) {
        if (copy_file(argv[i], fifo_name) != 0) {
            overall_status = EXIT_FAILURE;
        }
    }

    return overall_status;
}