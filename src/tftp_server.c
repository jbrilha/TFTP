#include "tftp_server.h"
#include "tftp.h"

int handle_req(int s, tftp_pkt pkt, struct sockaddr *addr, socklen_t slen) {
    FILE *fd;
    char *filename = (char *)pkt.req.filename;

    fd = fopen(filename, "r");
    if (fd == NULL) {
        perror("server: handle_req");
        char *err_str = "File not found";
        tftp_send_error(s, FILE_NOT_FOUND, err_str, addr, slen);
        exit(1);
    }

    uint8_t data[BLOCK_SIZE];
    size_t bytes_read;
    uint16_t block_nr = 0;
    bool should_close = false;

    while (!should_close) {
        puts("Sending...");
        bytes_read = fread(data, 1, BLOCK_SIZE, fd);
        if (bytes_read < BLOCK_SIZE) {
            should_close = true;
        }

        if (tftp_send_data(s, block_nr, data, bytes_read, addr, slen) < 0) {
            exit(EXIT_FAILURE);
        }

        if (tftp_recv(s, &pkt, 0, addr, &slen) < 0) {
            exit(EXIT_FAILURE);
        }
        switch (ntohs(pkt.opcode)) {
        case ACK:
            printf("ACK: %d\n", ntohs(pkt.ack.block_nr));
            block_nr++;
            continue;
        case ERROR:
            printf("ERROR: %s\n", (char *)pkt.error.error_str);
            break;
        default:
            printf("%d shit\n", ntohs(pkt.opcode));
        }
    }
    printf("Sent %d blocks of data.\n", block_nr);

    return 1;
}

int main(int argc, char **argv) {
    int s = create_ipv4_socket();
    if (s < 0) {
        exit(EXIT_FAILURE);
    }
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

    socklen_t len;
    while (1) {
        tftp_pkt pkt;
        len = sizeof(c_addr_in);

        if (tftp_recv(s, &pkt, 0, c_addr, &len) < 0) {
            exit(EXIT_FAILURE);
        }

        switch (ntohs(pkt.opcode)) {
        case RRQ:
        case WRQ:
            printf("REQ: %s\n", (char *)pkt.req.filename);
            handle_req(s, pkt, c_addr, len);
            break;
        case DATA:
            printf("DATA: %s\n", (char *)pkt.data.data);
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

    close(s);
}
