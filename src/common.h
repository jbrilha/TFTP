#ifndef COMMON_H
#define COMMON_H

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>

int create_ipv6_socket(void);

int create_ipv4_socket(void);

struct sockaddr_in6 init_ipv6_addr(uint16_t port, bool is_server);

struct sockaddr_in init_ipv4_addr(uint16_t port, bool is_server);

int allow_ipv4(int sock);

#endif /* COMMON_H */
