#include "tftp_server.h"

int handle_WRQ(int s, tftp_pkt pkt, struct sockaddr *addr, socklen_t slen) {}
ssize_t handle_RRQ(int s, tftp_pkt pkt, struct sockaddr *addr, socklen_t slen) {
    FILE *fd;
    char *filename = (char *)pkt.req.filename;
    ssize_t r;

    fd = fopen(filename, "r");
    if (fd == NULL) {
        perror("server: handle_req");
        char *err_str;
        uint16_t err_code;
        if (errno == ENOENT) {
            err_str = "File not found.";
            err_code = FILE_NOT_FOUND;
        } else if (errno == EACCES) {
            err_str = "Insufficient permissions to read file.";
            err_code = ACCESS_VIOLATION;
        } else {
            err_str = "Uknown file error.";
            err_code = UNDEFINED;
        }

        return tftp_send_error(s, err_code, err_str, addr, slen);
    }

    uint8_t data[BLOCK_SIZE];
    size_t dlen;
    uint16_t block_nr = 0;
    bool should_close = false;

    while (!should_close) {
        dlen = fread(data, 1, BLOCK_SIZE, fd);
        printf("Sending %zu bytes...\n", dlen);
        block_nr++;
        if (dlen < BLOCK_SIZE) {
            should_close = true;
        }

        if ((r = tftp_send_data(s, block_nr, data, dlen, addr, slen)) < 0) {
            return r;
        }

        if (tftp_recv(s, &pkt, 0, addr, &slen) < 0) {
            return r;
        }

        switch (ntohs(pkt.opcode)) {
        case ACK:
            printf("ACK: %d\n", ntohs(pkt.ack.block_nr));
            continue;
        case ERROR:
            printf("ERROR: %s\n", (char *)pkt.error.error_str);
            break;
        default:
            printf("%d shit\n", ntohs(pkt.opcode));
        }
    }

    printf("Sent %d blocks of data.\n", block_nr);
    fclose(fd);

    return r;
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

        printf("%d\n", dlen);

        switch (ntohs(pkt.opcode)) {
        case RRQ:
            printf("RRQ for: %s\n", (char *)pkt.req.filename);
            if ((r = handle_RRQ(s, pkt, c_addr, slen)) < 0) {
                printf("RRQ failed: %zd\n", r);
            }
            printf("RRQ success: %zd\n", r);
            break;
        case WRQ:
            printf("WRQ: %s\n", (char *)pkt.req.filename);
            handle_WRQ(s, pkt, c_addr, slen);
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
            printf("Invalid opcode: <%d>\nExpected RRQ or WRQ\nTerminating...",
                   ntohs(pkt.opcode));
        }
    }
}

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
