#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#define PORT 5000

int main(int argc, char **argv) {
  char buf[100];
  char *msg = "serverside";

  int sock;
  socklen_t len;
  struct sockaddr_in6 server_addr, client_addr;
  memset(&server_addr, 0, sizeof(server_addr));

  server_addr.sin6_family = AF_INET6;
  server_addr.sin6_port = htons(PORT);
  server_addr.sin6_addr = in6addr_any;

  sock = socket(AF_INET6, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("Failed to create socket\n");
    exit(EXIT_FAILURE);
  }

  int off = 0;
  // should accept IPv4 as well
  if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) < 0) {
    perror("setsockopt IPV6_V6ONLY");
  }

  int b = bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (b < 0) {
    perror("Failed to bind socket\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    len = sizeof(client_addr);
    int n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr,
                     &len);
    if (n < 0) {
      perror("Failed to receive msg\n");
      exit(EXIT_FAILURE);
    }
    buf[n] = '\0';
    puts(buf);
  }
}
