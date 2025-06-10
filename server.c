#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
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

    //send cmd to all group clients
    void send_to_clients(char *cmd, int *client_indices, int count) {
        for (int j = 0; j < count; j++) {
            int i = client_indices[j];
            if (i >= 0 && i < MAX_CLIENTS && client_sockets[i] > 0) {
                write(client_sockets[i], cmd, strlen(cmd) + 1);
            }
        }
    }

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

        //new connection
        if (FD_ISSET(sfd, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t len = sizeof(client_addr);
            int new_socket = accept(sfd, (struct sockaddr*)&client_addr, &len);
            if (new_socket < 0) {
                perror("accept");
                continue;
            }

            //add to clients array
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    printf("Client %d connected\n", i);
		    //printf("> ");
                    break;
                }
            }
	    //printf("> ");
        }

        //user input
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char cmd[1024];
	    int group[MAX_CLIENTS];
	    int group_count = 0;
            if (!fgets(cmd, sizeof(cmd), stdin)) break;
            cmd[strcspn(cmd, "\n")] = '\0';

            if (strncmp(cmd, "cmb", 3) == 0) { 
                if (cmd[3] != ' ' && cmd[3] != '\0') {
                    //printf("Invalid command. Expected 'cmb' followed by space or end of line.\n");
                } else {
                    char *rest = cmd + 3;
                    while (*rest == ' ') rest++;

                    char *token = strtok(rest, " ");
                    while (token != NULL && group_count < MAX_CLIENTS) {
                        char *endptr;
                        long val = strtol(token, &endptr, 10);
                        if (*endptr == '\0' && val >= 0 && val < MAX_CLIENTS) {
                            group[group_count++] = (int)val;
                        }
                        token = strtok(NULL, " ");
                     }

		    printf("Group was created. Number of clients: %d\n", group_count);
                    printf("Clients: ");
                    for (int i = 0; i < group_count; i++) printf("%d ", group[i]);
                    printf("\n");

                     //get group command
		     if (!fgets(cmd, sizeof(cmd), stdin)) break;
                     cmd[strcspn(cmd, "\n")] = '\0';
		     send_to_clients(cmd, group, group_count);
                }
            } else {
		int all[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}; //!
	        send_to_clients(cmd, all, MAX_CLIENTS);
	    }
        }

        //client responses
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
