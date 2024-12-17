#include "tftp_server.h"

int main(int argc, char **argv) {
    int s = create_ipv4_socket();
    if (s < 0) {
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in s_addr_in = init_ipv4_addr(PORT, true);
    // struct sockaddr_in6 s_addr_in = init_ipv6_addr(PORT, true);
    struct sockaddr *s_addr = (struct sockaddr *)&s_addr_in;
    // allow_ipv4(sock);

    int b = bind(s, s_addr, sizeof(struct sockaddr));
    if (b < 0) {
        perror("Failed to bind socket\n");
        exit(EXIT_FAILURE);
    }

    recv_reqs(s);

    close(s);
}

ssize_t recv_reqs(int s) {
    struct sockaddr_in c_addr_in;
    struct sockaddr *c_addr = (struct sockaddr *)&c_addr_in;

    socklen_t slen, dlen;
    ssize_t r;
    while (1) {
        tftp_pkt pkt;
        slen = sizeof(c_addr_in);

        if ((dlen = tftp_recv(s, &pkt, 0, c_addr, &slen)) < 0) {
            exit(EXIT_FAILURE);
        }

        uint16_t opcode = ntohs(pkt.opcode);
        switch (opcode) {
        case RRQ:
            printf("RRQ for: %s\n", (char *)pkt.req.filename);
            if ((r = handle_read(s, &pkt, c_addr, slen)) < 0) {
                printf("RRQ failed with ERROR: %s\n",
                       errcode_to_str(ntohs(pkt.error.error_code)));
                continue;
            }
            break;
        case WRQ:
            printf("WRQ for: %s\n", (char *)pkt.req.filename);
            if ((r = handle_write(s, &pkt, c_addr, slen)) < 0) {
                printf("WRQ failed with ERROR: %s\n",
                       errcode_to_str(ntohs(pkt.error.error_code)));
                continue;
            }
            break;
        // case DATA:
        //     printf("DATA: %s\n", (char *)pkt.data.data);
        //     break;
        // case ACK:
        //     printf("ACK: %d\n", ntohs(pkt.ack.block_nr));
        //     break;
        // case ERROR:
        //     printf("ERROR: %s\n", (char *)pkt.error.error_str);
        //     break;
        default:
            printf("Invalid opcode: <%d>\nExpected RRQ or WRQ.\n",
                   ntohs(pkt.opcode));
            continue;
        }
        printf("%s success\n", opcode_to_str(opcode));
    }
}

