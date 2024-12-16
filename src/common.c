#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_socklen_t.h>
#include <sys/_types/_ssize_t.h>
#include <sys/_types/_u_int32_t.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

int create_ipv6_socket(void) {
    int s;
    if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
        perror("Failed to create socket\n");
    }
    return s;
}

int create_ipv4_socket(void) {
    int s;
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Failed to create socket\n");
    }
    return s;
}

struct sockaddr_in6 init_ipv6_addr(uint16_t port, bool is_server) {
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    struct in6_addr INADDR = is_server ? in6addr_any : in6addr_loopback;

    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    addr.sin6_addr = INADDR;

    // ::1 is localhost, equivalent to above, should set an actual IP otherwise
    // if (inet_pton(AF_INET6, "::1", &addr.sin6_addr) <= 0) {
    //     perror("inet_pton failed");
    //     exit(EXIT_FAILURE);
    // }

    return addr;
}

int allow_ipv4(int sock) {
    int off = 0;
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) < 0) {
        perror("setsockopt IPV6_V6ONLY");
        return -1;
    }

    return 0;
}

struct sockaddr_in init_ipv4_addr(uint16_t port, bool is_server) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    u_int32_t INADDR = is_server ? INADDR_ANY : INADDR_LOOPBACK;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR);

    // equivalent to above, should set an actual IP otherwise
    // if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0) {
    //     perror("inet_pton failed");
    //     exit(EXIT_FAILURE);
    // }

    return addr;
}
