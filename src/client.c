#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 5000

int main(int argc, char **argv) {
    char buf[100];
    char *msg = "clientside";

    int sock;
    struct sockaddr_in6 server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(PORT);
    server_addr.sin6_addr = in6addr_loopback;
    
    // ::1 is localhost, should bind to actual IP otherwise
    /* if (inet_pton(AF_INET6, "::1", &server_addr.sin6_addr) <= 0) {
        perror("inet_pton failed");
        exit(EXIT_FAILURE);
    } */

    sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Failed to create socket\n");
        exit(EXIT_FAILURE);
    }

    int c = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (c < 0) {
        perror("Failed to connect socket\n");
        exit(EXIT_FAILURE);
    }

    int n = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)NULL, 0);
    if (n < 0) {
        perror("Failed to send msg\n");
        exit(EXIT_FAILURE);
    }

    close(sock);
}
