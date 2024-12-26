#include "tftp_server.h"
#include "tftp.h"

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
    exit(EXIT_SUCCESS);
}

ssize_t recv_reqs(int s) {
    struct sockaddr_in c_addr_in;
    struct sockaddr *c_addr = (struct sockaddr *)&c_addr_in;
    bool file_is_open = false;

    socklen_t slen, dlen;
    ssize_t r;
    char *filename;
    const char *err_str;
    uint16_t err_code;
    FILE *fd;
        char *filename = (char *)pkt->req.filename;
        fd = open_file(filename, "r", &err_code, &err_str);
        if (fd == NULL) {
            tftp_send_error(s, pkt, err_code, err_str, addr, slen);
            close(s);
            return;
        }
        char *filename = (char *)pkt->req.filename;
        fd = open_file(filename, "wx", &err_code, &err_str);
        if (fd == NULL) {
            tftp_send_error(s, pkt, err_code, err_str, addr, slen);
            close(s);
            return;
        }

    while (1) {
        tftp_pkt pkt;
        slen = sizeof(c_addr_in);

        if ((dlen = tftp_recv(s, &pkt, 0, c_addr, &slen)) < 0) {
            exit(EXIT_FAILURE);
        }

        uint16_t opcode = ntohs(pkt.opcode);
        switch (opcode) {
        case RRQ:
            filename = (char *)pkt.req.filename;
            fd = fopen(filename, "r");
            if (fd == NULL) {
                perror("RRQ");
                handle_file_error(&err_code, &err_str);
                printf("Could not open file '%s' for reading: [%d] %s\n",
                       filename, err_code, err_str);
                tftp_send_error(s, &pkt, err_code, err_str, c_addr, slen);
                continue;
            } else {
                file_is_open = true;
            }
            printf("RRQ for: %s\n", (char *)pkt.req.filename);
            if ((r = handle_read(s, &pkt, c_addr, slen, fd)) < 0) {
                printf("RRQ failed with ERROR: %s\n",
                       errcode_to_str(ntohs(pkt.error.error_code)));
            }
            break;
        case WRQ:
            filename = (char *)pkt.req.filename;
            printf("WRQ for: %s\n", filename);
            fd = fopen(filename, "wx");
            if (fd == NULL) {
                perror("handle_write");
                handle_file_error(&err_code, &err_str);
                printf("Could not open file '%s' for writing: [%d] %s\n",
                       filename, err_code, err_str);
                tftp_send_error(s, &pkt, err_code, err_str, c_addr, slen);
                continue;
            } else {
                file_is_open = true;
            }

            if (tftp_send_ack(s, &pkt, 0, c_addr, slen) < 0) {
                perror("Failed to send ACK");
            }

            if ((r = handle_write(s, &pkt, c_addr, slen, fd)) < 0) {
                printf("WRQ failed with ERROR: %s\n",
                       errcode_to_str(ntohs(pkt.error.error_code)));
                remove(filename);
            }
            break;
        default:
            printf("Invalid opcode: <%d>\nExpected RRQ or WRQ.\n",
                   ntohs(pkt.opcode));
            continue;
        }
        printf("%s success\n", opcode_to_str(opcode));
        if (file_is_open) {
            file_is_open = false;
            fclose(fd);
        }
    }
}
