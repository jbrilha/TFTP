#include "tftp_server.h"

int main(int argc, char **argv) {
    int s = create_ipv4_socket();
    socklen_t len;
    struct sockaddr_in c_addr_in;
    struct sockaddr_in s_addr_in = init_ipv4_addr(PORT, true);
    // struct sockaddr_in6 s_addr_in = init_ipv6_addr(PORT, true);
    struct sockaddr *s_addr = (struct sockaddr *)&s_addr_in;
    struct sockaddr *c_addr = (struct sockaddr *)&c_addr_in;
    // allow_ipv4(sock);

    int b = bind(s, s_addr, sizeof(struct sockaddr));
    if (b < 0) {
        perror("Failed to bind socket\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        tftp_pkt pkt;
        len = sizeof(c_addr_in);
        int n = recvfrom(s, &pkt, sizeof(pkt), 0, c_addr, &len);
        if (n < 0) {
            perror("Failed to receive msg\n");
            exit(EXIT_FAILURE);
        }

        switch (ntohs(pkt.opcode)) {
        case RRQ:
        case WRQ:
            printf("REQ: %s", (char *)pkt.req.filename);
            break;
        case DATA:
            printf("DATA: %s", (char *)pkt.data.data);
            break;
        case ACK:
            printf("ACK: %d", ntohs(pkt.ack.block_nr));
            break;
        case ERROR:
            printf("ERROR: %s", (char *)pkt.error.error_str);
            break;
        default:
            printf("%d shit", ntohs(pkt.opcode));
        }
        printf("\n");
    }
}
