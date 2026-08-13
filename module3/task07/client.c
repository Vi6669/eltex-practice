#include "client.h"

volatile sig_atomic_t running = 1;

void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

int init_client_socket(const char *ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Ошибка при создании сокета");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Некорректный IP-адрес сервера\n");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Ошибка при подключении к серверу");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

const char *get_base_filename(const char *path) {
    const char *base = strrchr(path, '/');
    if (!base) base = strrchr(path, '\\');
    return base ? base + 1 : path;
}

void send_packet(int sockfd, MsgType type, const char *nickname, const char *text, const char *filename, uint32_t data_len) {
    ChatPacket packet;
    memset(&packet, 0, sizeof(packet));
    packet.type = type;
    packet.data_len = data_len;
    strncpy(packet.nickname, nickname, MAX_NICK_LEN - 1);
    if (text) strncpy(packet.payload, text, MAX_CHUNK_SIZE - 1);
    if (filename) strncpy(packet.filename, filename, MAX_FILENAME - 1);
    
    write_all(sockfd, &packet, sizeof(packet));
}

void upload_file(int sockfd, const char *nickname, const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        printf("\r\033[31mОшибка: не удалось открыть файл '%s'\033[0m\n> ", filepath);
        fflush(stdout);
        return;
    }

    const char *filename = get_base_filename(filepath);
    send_packet(sockfd, MSG_FILE_START, nickname, NULL, filename, 0);
    printf("\r\033[33m[~] Отправка файла '%s'...\033[0m\n", filename);

    char buffer[MAX_CHUNK_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, MAX_CHUNK_SIZE, f)) > 0) {
        send_packet(sockfd, MSG_FILE_DATA, nickname, buffer, filename, bytes_read);
        memset(buffer, 0, MAX_CHUNK_SIZE);
    }

    send_packet(sockfd, MSG_FILE_END, nickname, NULL, filename, 0);
    fclose(f);

    printf("\r\033[32m[+] Файл '%s' успешно отправлен!\033[0m\n> ", filename);
    fflush(stdout);
}

void process_user_input(int sockfd, const char *nickname) {
    char buffer[MAX_CHUNK_SIZE];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) return;

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') buffer[len - 1] = '\0';

    if (strlen(buffer) == 0) {
        printf("> ");
        fflush(stdout);
        return;
    }

    if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "quit") == 0) {
        running = 0;
        return;
    }

    if (strncmp(buffer, "/file ", 6) == 0) {
        upload_file(sockfd, nickname, buffer + 6);
    } else {
        printf("\033[34m[Вы]:\033[0m %s\n> ", buffer);
        fflush(stdout);
        send_packet(sockfd, MSG_CHAT, nickname, buffer, NULL, 0);
    }
}

void handle_server_message(int sockfd, FILE **rx_file) {
    ChatPacket packet;
    ssize_t n = read_all(sockfd, &packet, sizeof(packet));
    if (n <= 0) {
        printf("\r\033[31m[-] Соединение с сервером потеряно.\033[0m\n");
        running = 0;
        return;
    }

    packet.nickname[MAX_NICK_LEN - 1] = '\0';
    packet.filename[MAX_FILENAME - 1] = '\0';

    switch (packet.type) {
        case MSG_JOIN:
            printf("\r\033[32m[+] %s вошел в чат\033[0m\n", packet.nickname);
            break;
        case MSG_CHAT:
            packet.payload[MAX_CHUNK_SIZE - 1] = '\0';
            printf("\r\033[36m[%s]:\033[0m %s\n", packet.nickname, packet.payload);
            break;
        case MSG_LEAVE:
            printf("\r\033[31m[-] %s вышел из чата\033[0m\n", packet.nickname);
            break;
        case MSG_FILE_START: {
            char out_path[512];
            snprintf(out_path, sizeof(out_path), "received_%s", packet.filename);
            *rx_file = fopen(out_path, "wb");
            if (*rx_file) {
                printf("\r\033[32m[+] Принимаем файл '%s' от %s...\033[0m\n", packet.filename, packet.nickname);
            } else {
                printf("\r\033[31mОшибка создания файла %s для записи\033[0m\n", out_path);
            }
            break;
        }
        case MSG_FILE_DATA:
            if (*rx_file) fwrite(packet.payload, 1, packet.data_len, *rx_file);
            break;
        case MSG_FILE_END:
            if (*rx_file) {
                fclose(*rx_file);
                *rx_file = NULL;
                printf("\r\033[32m[+] Файл сохранен: 'received_%s'\033[0m\n", packet.filename);
            }
            break;
        default: break;
    }
    printf("\r> ");
    fflush(stdout);
}

void run_client_loop(int sockfd, const char *nickname) {
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    FILE *rx_file = NULL;

    while (running) {
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            if (errno == EINTR) break;
            perror("Ошибка poll");
            break;
        }

        if (fds[1].revents & POLLIN) {
            handle_server_message(sockfd, &rx_file);
        }

        if (fds[0].revents & POLLIN) {
            process_user_input(sockfd, nickname);
        }
    }

    if (rx_file) fclose(rx_file);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Использование: %s <server_ip> <port> <nickname>\n", argv[0]);
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    int sockfd = init_client_socket(argv[1], atoi(argv[2]));
    if (sockfd < 0) return 1;

    send_packet(sockfd, MSG_JOIN, argv[3], NULL, NULL, 0);

    printf("=== TCP Групповой Чат ===\n");
    printf("Отправить файл: /file <путь>\nВыход: exit\n=========================\n\n> ");
    fflush(stdout);

    run_client_loop(sockfd, argv[3]);

    printf("\nВыход из чата...\n");
    send_packet(sockfd, MSG_LEAVE, argv[3], NULL, NULL, 0);
    close(sockfd);
    
    return 0;
}