#include "tftp_client.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/_types/_ssize_t.h>

int main(int argc, char **argv) {
    printf("oiu0;\n");
    int s = create_ipv4_socket();
    struct sockaddr_in s_addr_in = init_ipv4_addr(PORT, false);
    // struct sockaddr_in6 s_addr_in = init_ipv6_addr(PORT, false);
    socklen_t socklen = sizeof(s_addr_in);
    struct sockaddr *s_addr = (struct sockaddr *)&s_addr_in;

    // unnecessary for udp
    /* int c = connect(sock, (struct sockaddr *)&server_addr,
    sizeof(server_addr)); if (c < 0) { perror("Failed to connect socket\n");
        exit(EXIT_FAILURE);
    } */

    uint16_t op = RRQ;
    char *filename = "file.txt";
    char *mode = "octet";
    printf("oiu1;\n");
    if (tftp_send_req(s, op, filename, mode, s_addr, socklen) < 0) {
        exit(EXIT_FAILURE);
    }

    // socklen_t len;
    tftp_pkt pkt;
    // len = sizeof(s_addr_in);

    FILE *fd;
    fd = fopen("file_client.txt", "w");
    if (fd == NULL) {
        perror("client: openfile");
        exit(1);
    }

    bool should_close = false;
    ssize_t dlen;
    ssize_t recv_len;
    while (!should_close) {
        if ((recv_len = tftp_recv(s, &pkt, 0, s_addr, &socklen)) < 0) {
            exit(EXIT_FAILURE);
        }
        printf("Raw received length: %zd\n", recv_len);
        printf("Size of tftp_pkt: %zu\n", sizeof(tftp_pkt));
        // need to subtract 4 because union makes each pkt expand to the size
        // of a req pkt, meeaning there's always 2 extra bytes (the terminator)
        dlen = recv_len - 4;
        uint16_t ack;
        switch (ntohs(pkt.opcode)) {
        case DATA:
            ack = ntohs(pkt.data.block_nr);
            if (fwrite(pkt.data.data, 1, dlen, fd) != dlen) {
                perror("fwrite failed");
                exit(EXIT_FAILURE);
            }
            fflush(fd);
            if (tftp_send_ack(s, ack, s_addr, socklen) < 0) {
                exit(EXIT_FAILURE);
            }
            printf("DATA — bnr:%u / %d\n", ntohs(pkt.data.block_nr), dlen);
            break;
        case ACK:
            printf("ACK: %d\n", ntohs(pkt.ack.block_nr));
            break;
        case ERROR:
            printf("ERROR: %s\n", (char *)pkt.error.error_str);
            break;
        default:
            printf("%d shit\n", ntohs(pkt.opcode));
        }
    }
    // char *data = "dataAAAAAA";
    // size_t data_len = strlen(data);
    // uint16_t blocknr = 1;
    // if (tftp_send_data(s, blocknr, data, data_len, s_addr, socklen) < 0)
    // {
    //     exit(EXIT_FAILURE);
    // }
    //
    // char *err = "errorrrrrr";
    // uint16_t errcode = 15;
    // if (tftp_send_error(s, errcode, err, s_addr, socklen) < 0) {
    //     exit(EXIT_FAILURE);
    // }
    //
    // // uint16_t ack = 5;
    // if (tftp_send_ack(s, ack, s_addr, socklen) < 0) {
    //     exit(EXIT_FAILURE);
    // }

    close(s);
}
