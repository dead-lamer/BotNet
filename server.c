#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#define MAX_CLIENTS 10

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    const char *service = argv[2];
    struct addrinfo hints = {0};
    struct addrinfo *result;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int err = getaddrinfo(host, service, &hints, &result);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return 1;
    }

    int sfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sfd == -1) {
        perror("socket");
        freeaddrinfo(result);
        return 1;
    }

    if (bind(sfd, result->ai_addr, result->ai_addrlen) == -1) {
        perror("bind");
        close(sfd);
        freeaddrinfo(result);
        return 1;
    }

    if (listen(sfd, SOMAXCONN) == -1) {
        perror("listen");
        close(sfd);
        freeaddrinfo(result);
        return 1;
    }

    freeaddrinfo(result);

    int client_sockets[MAX_CLIENTS] = {0};
    fd_set readfds;

    printf("Server running...\n");

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        int max_fd = sfd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] > 0) FD_SET(client_sockets[i], &readfds);
            if (client_sockets[i] > max_fd) max_fd = client_sockets[i];
        }

        if (select(max_fd+1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select error");
            break;
        }

        // New connection
        if (FD_ISSET(sfd, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t len = sizeof(client_addr);
            int new_socket = accept(sfd, (struct sockaddr*)&client_addr, &len);
            if (new_socket < 0) {
                perror("accept");
                continue;
            }

            // Add to clients array
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    printf("Client %d connected\n", i);
                    break;
                }
            }
        }

        // User input
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char cmd[1024];
            if (!fgets(cmd, sizeof(cmd), stdin)) break;
            cmd[strcspn(cmd, "\n")] = '\0';

            // Send to all clients
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] > 0) {
                    write(client_sockets[i], cmd, strlen(cmd)+1);
                }
            }
        }

        // Client responses
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sock = client_sockets[i];
            if (sock > 0 && FD_ISSET(sock, &readfds)) {
                char buffer[1024];
                int bytes = read(sock, buffer, sizeof(buffer));
                if (bytes <= 0) {
                    printf("Client %d disconnected\n", i);
                    close(sock);
                    client_sockets[i] = 0;
                } else {
                    buffer[bytes] = '\0';
                    printf("Client %d response:\n%s\n", i, buffer);
                }
            }
        }
    }

    close(sfd);
    return 0;
}
