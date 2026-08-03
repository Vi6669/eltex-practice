#include "tester.h"
#include "copier.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int tests_run = 0;
int tests_failed = 0;

// Вспомогательная функция для создания тестового файла с заданным текстом
static int create_test_file(const char *path, const char *content) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return 0;
    size_t len = strlen(content);
    ssize_t written = write(fd, content, len);
    close(fd);
    return (written == (ssize_t)len);
}

// Вспомогательная функция для побайтового сравнения двух файлов
static int files_equal(const char *path1, const char *path2) {
    int fd1 = open(path1, O_RDONLY);
    int fd2 = open(path2, O_RDONLY);
    if (fd1 < 0 || fd2 < 0) {
        if (fd1 >= 0) close(fd1);
        if (fd2 >= 0) close(fd2);
        return 0;
    }

    char buf1[512];
    char buf2[512];
    ssize_t r1, r2;
    int equal = 1;

    while (1) {
        r1 = read(fd1, buf1, sizeof(buf1));
        r2 = read(fd2, buf2, sizeof(buf2));
        if (r1 < 0 || r2 < 0 || r1 != r2) {
            equal = 0;
            break;
        }
        if (r1 == 0) {
            break; // Достигнут конец обоих файлов (EOF)
        }
        if (memcmp(buf1, buf2, r1) != 0) {
            equal = 0;
            break;
        }
    }

    close(fd1);
    close(fd2);
    return equal;
}

// Тест 1: Неименованный канал - Копирование одного файла
static void test_unnamed_single() {
    ASSERT_TRUE("Failed to create test file", create_test_file("test_f1.txt", "Hello from test 1!"));
    int res = copy_file("test_f1.txt", NULL);
    ASSERT_TRUE("copy_file returned error status", res == 0);
    ASSERT_TRUE("Copied file is not identical to original", files_equal("test_f1.txt", "test_f1.txt.copy"));
}

// Тест 2: Неименованный канал - Копирование нескольких файлов
static void test_unnamed_multiple() {
    ASSERT_TRUE("Failed to create file 2", create_test_file("test_f2.txt", "Content of file 2"));
    ASSERT_TRUE("Failed to create file 3", create_test_file("test_f3.txt", "Short content 3"));
    
    int res1 = copy_file("test_f2.txt", NULL);
    int res2 = copy_file("test_f3.txt", NULL);
    
    ASSERT_TRUE("First copy failed", res1 == 0);
    ASSERT_TRUE("Second copy failed", res2 == 0);
    ASSERT_TRUE("File 2 copy mismatch", files_equal("test_f2.txt", "test_f2.txt.copy"));
    ASSERT_TRUE("File 3 copy mismatch", files_equal("test_f3.txt", "test_f3.txt.copy"));
}

// Тест 3: Неименованный канал - Копирование копии 3 раза подряд
static void test_unnamed_nested() {
    // f1.txt -> copy (1) -> copy.copy (2) -> copy.copy.copy (3)
    int res1 = copy_file("test_f1.txt.copy", NULL);
    int res2 = copy_file("test_f1.txt.copy.copy", NULL);
    int res3 = copy_file("test_f1.txt.copy.copy.copy", NULL);

    ASSERT_TRUE("Nested copy 1 failed", res1 == 0);
    ASSERT_TRUE("Nested copy 2 failed", res2 == 0);
    ASSERT_TRUE("Nested copy 3 failed", res3 == 0);
    ASSERT_TRUE("Nested copy integrity check failed", files_equal("test_f1.txt", "test_f1.txt.copy.copy.copy.copy"));
}

// Тест 4: Именованный канал (FIFO) - Копирование одного файла
static void test_named_single() {
    int res = copy_file("test_f1.txt", "test_fifo");
    ASSERT_TRUE("FIFO copy returned error status", res == 0);
    ASSERT_TRUE("FIFO copied file is not identical to original", files_equal("test_f1.txt", "test_f1.txt.copy"));
}

// Тест 5: Именованный канал (FIFO) - Копирование нескольких файлов
static void test_named_multiple() {
    int res1 = copy_file("test_f2.txt", "test_fifo");
    int res2 = copy_file("test_f3.txt", "test_fifo");
    
    ASSERT_TRUE("FIFO copy 1 failed", res1 == 0);
    ASSERT_TRUE("FIFO copy 2 failed", res2 == 0);
    ASSERT_TRUE("FIFO file 2 copy mismatch", files_equal("test_f2.txt", "test_f2.txt.copy"));
    ASSERT_TRUE("FIFO file 3 copy mismatch", files_equal("test_f3.txt", "test_f3.txt.copy"));
}

// Тест 6: Именованный канал (FIFO) - Копирование копии 3 раза
static void test_named_nested() {
    // Удалим старые вложенные файлы перед тестом
    unlink("test_f1.txt.copy.copy");
    unlink("test_f1.txt.copy.copy.copy");
    unlink("test_f1.txt.copy.copy.copy.copy");

    int res1 = copy_file("test_f1.txt.copy", "test_fifo");
    int res2 = copy_file("test_f1.txt.copy.copy", "test_fifo");
    int res3 = copy_file("test_f1.txt.copy.copy.copy", "test_fifo");

    ASSERT_TRUE("FIFO nested copy 1 failed", res1 == 0);
    ASSERT_TRUE("FIFO nested copy 2 failed", res2 == 0);
    ASSERT_TRUE("FIFO nested copy 3 failed", res3 == 0);
    ASSERT_TRUE("FIFO nested copy integrity check failed", files_equal("test_f1.txt", "test_f1.txt.copy.copy.copy.copy"));
}

// Тест 7: Пограничный случай - Попытка скопировать несуществующий файл
static void test_nonexistent_file() {
    int res = copy_file("test_missing_file_xyz.txt", NULL);
    ASSERT_TRUE("Copying non-existent file should return error (-1)", res < 0);
}

// Тест 8: Объемный бинарный файл (PDF-отчет из модуля 4)
static void test_pdf_volume() {
    struct stat st;
    if (stat("test_document.pdf", &st) < 0) {
        // Если файла нет, просто пропускаем тест (это не ошибка тестера)
        printf("s"); 
        return;
    }

    // Тестируем копирование тяжелого бинарника через неименованный канал
    int res = copy_file("test_document.pdf", NULL);
    ASSERT_TRUE("PDF copy via pipe failed", res == 0);
    ASSERT_TRUE("Copied PDF is not binary-identical to original", files_equal("test_document.pdf", "test_document.pdf.copy"));
}

int main() {
    printf("Running C-based unit tests: ");
    fflush(stdout);

    RUN_TEST(test_unnamed_single);
    RUN_TEST(test_unnamed_multiple);
    RUN_TEST(test_unnamed_nested);
    RUN_TEST(test_named_single);
    RUN_TEST(test_named_multiple);
    RUN_TEST(test_named_nested);
    RUN_TEST(test_nonexistent_file);
    RUN_TEST(test_pdf_volume);

    printf("\nTests run: %d, Failures: %d\n", tests_run, tests_failed);
    
    if (tests_failed > 0) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}