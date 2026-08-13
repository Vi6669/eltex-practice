#include "server.h"

int client_sockets[MAX_CLIENTS];
int num_clients = 0;

void remove_client(int client_fd) {
    for (int i = 0; i < num_clients; i++) {
        if (client_sockets[i] == client_fd) {
            client_sockets[i] = client_sockets[num_clients - 1];
            num_clients--;
            break;
        }
    }
}

int init_server_socket(int port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("Ошибка при создании сокета");
        return -1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Ошибка bind");
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, 10) < 0) {
        perror("Ошибка listen");
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

int init_epoll(int listen_fd) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("Ошибка epoll_create1");
        return -1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("Ошибка epoll_ctl для listen_fd");
        close(epoll_fd);
        return -1;
    }

    return epoll_fd;
}

void handle_new_connection(int epoll_fd, int listen_fd) {
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int client_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
    if (client_fd < 0) {
        perror("Ошибка accept");
        return;
    }

    if (num_clients >= MAX_CLIENTS) {
        fprintf(stderr, "Превышен лимит подключений\n");
        close(client_fd);
        return;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = client_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
        perror("Ошибка epoll_ctl ADD");
        close(client_fd);
        return;
    }

    client_sockets[num_clients++] = client_fd;
    char cli_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli_addr.sin_addr, cli_ip, INET_ADDRSTRLEN);
    printf("[+] Новое соединение с %s:%d\n", cli_ip, ntohs(cli_addr.sin_port));
}

void broadcast_packet(int sender_fd, const ChatPacket *packet) {
    for (int c = 0; c < num_clients; c++) {
        if (client_sockets[c] != sender_fd) {
            write_all(client_sockets[c], packet, sizeof(*packet));
        }
    }
}

void log_packet(const ChatPacket *packet) {
    if (packet->type == MSG_JOIN) {
        printf("[Вход] %s вошел в чат\n", packet->nickname);
    } else if (packet->type == MSG_LEAVE) {
        printf("[Выход] %s покинул чат\n", packet->nickname);
    } else if (packet->type == MSG_CHAT) {
        printf("[%s]: %s\n", packet->nickname, packet->payload);
    } else if (packet->type == MSG_FILE_START) {
        printf("[Файл] %s инициировал отправку файла: %s\n", packet->nickname, packet->filename);
    }
}

void handle_client_disconnect(int epoll_fd, int client_fd) {
    printf("[-] Соединение закрыто для дескриптора %d\n", client_fd);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    close(client_fd);
    remove_client(client_fd);
}

void handle_client_data(int epoll_fd, int client_fd) {
    ChatPacket packet;
    ssize_t n = read_all(client_fd, &packet, sizeof(packet));

    if (n <= 0) {
        handle_client_disconnect(epoll_fd, client_fd);
    } else {
        log_packet(&packet);
        broadcast_packet(client_fd, &packet);
    }
}

void run_server_loop(int epoll_fd, int listen_fd) {
    struct epoll_event events[MAX_EVENTS];
    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("Ошибка epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == listen_fd) {
                handle_new_connection(epoll_fd, listen_fd);
            } else {
                handle_client_data(epoll_fd, events[i].data.fd);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Использование: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    int listen_fd = init_server_socket(port);
    if (listen_fd < 0) return 1;

    int epoll_fd = init_epoll(listen_fd);
    if (epoll_fd < 0) {
        close(listen_fd);
        return 1;
    }

    printf("=== TCP Сервер чата запущен на порту %d ===\n", port);
    run_server_loop(epoll_fd, listen_fd);

    close(listen_fd);
    close(epoll_fd);
    return 0;
}