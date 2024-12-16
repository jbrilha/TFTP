#include "tftp_client.h"

ssize_t handle_recv(int s, struct sockaddr *s_addr, char *filename,
                    socklen_t slen) {
    tftp_pkt pkt;
    FILE *fd;
    fd = fopen("client_file.txt", "w");
    if (fd == NULL) {
        perror("server: handle_req");
        char *err_str;
        uint16_t err_code;
        if (errno == EEXIST) {
            err_str = "File already exists.";
            err_code = FILE_EXISTS;
        } else if (errno == EACCES) {
            // should check this before even requesting tho
            err_str = "Insufficient permissions to write file.";
            err_code = ACCESS_VIOLATION;
        } else if (errno == ENOSPC) {
                err_str = "Insufficient disk space";
                err_code = DISK_FULL;
        } else {
            err_str = "Uknown file error.";
            err_code = UNDEFINED;
        }

        return tftp_send_error(s, err_code, err_str, s_addr, slen);
    }

    bool should_close = false;
    ssize_t dlen;
    while (!should_close) {
        if ((dlen = tftp_recv(s, &pkt, 0, s_addr, &slen)) < 0) {
            exit(EXIT_FAILURE);
        }

        dlen -= 4;
        if (dlen < BLOCK_SIZE) {
            should_close = true;
        }

        uint16_t ack;
        switch (ntohs(pkt.opcode)) {
        case DATA:
            ack = ntohs(pkt.data.block_nr);
            if (fwrite(pkt.data.data, 1, dlen, fd) != dlen) {
                perror("fwrite failed");
                exit(EXIT_FAILURE);
            }
            fflush(fd);
            if (tftp_send_ack(s, ack, s_addr, slen) < 0) {
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
    fclose(fd);
}

int main(int argc, char **argv) {
    char *filename, *mode;
    uint16_t opcode;
    parse_args(argc, argv, &opcode, &filename, &mode);
    printf("%hu|%s|%s\n", opcode, filename, mode);

    int s = create_ipv4_socket();
    struct sockaddr_in s_addr_in = init_ipv4_addr(PORT, false);
    // struct sockaddr_in6 s_addr_in = init_ipv6_addr(PORT, false);
    socklen_t slen = sizeof(s_addr_in);
    struct sockaddr *s_addr = (struct sockaddr *)&s_addr_in;

    // unnecessary for udp
    /* int c = connect(sock, (struct sockaddr *)&server_addr,
    sizeof(server_addr)); if (c < 0) { perror("Failed to connect socket\n");
        exit(EXIT_FAILURE);
    } */

    if (tftp_send_req(s, opcode, filename, mode, s_addr, slen) < 0) {
        exit(EXIT_FAILURE);
    }

    handle_recv(s, s_addr, filename, slen);

    // char *data = "dataAAAAAA";
    // size_t data_len = strlen(data);
    // uint16_t blocknr = 1;
    // if (tftp_send_data(s, blocknr, data, data_len, s_addr, slen) < 0) {
    //     exit(EXIT_FAILURE);
    // }
    //
    // char *err = "errorrrrrr";
    // uint16_t errcode = 15;
    // if (tftp_send_error(s, errcode, err, s_addr, slen) < 0) {
    //     exit(EXIT_FAILURE);
    // }
    //
    // uint16_t ack = 5;
    // if (tftp_send_ack(s, ack, s_addr, slen) < 0) {
    //     exit(EXIT_FAILURE);
    // }

    close(s);
}

void parse_args(int argc, char **argv, uint16_t *opcode, char **filename,
                char **mode) {
    if (argc < 3 || !argv[1][0] || !argv[2][0]) {
        puts("Usage: tftp_client <opcode> <filename> <mode>\n\n"
             "Examples:\n"
             "\ttftp_client RRQ server_file.txt octet\n"
             "\ttftp_client WRQ client_file.txt NetAscii\n\n"
             "<opcode> can only be 'RRQ' / 'WRQ' or 1 / 2 respectively\n"
             "<mode> is case insensitive and optional, defaults to 'octet'");
        exit(EXIT_FAILURE);
    }

    if (isdigit(argv[1][0])) {
        *opcode = atoi(argv[1]);
    } else {
        *opcode = str_to_opcode(argv[1]);
    }
    if (*opcode < 1 || *opcode > 2) {
        puts("Invalid opcode."
             "\n<opcode> can only be 'RRQ' / 'WRQ' or 1 / 2 respectively");
        exit(EXIT_FAILURE);
    }

    *filename = trim(argv[2]);
    if (!strcmp(*filename, "")) {
        puts("Filename cannot be empty.");
        exit(EXIT_FAILURE);
    }

    if (argc == 3) {
        *mode = "octet";
    } else {
        if (!validate_mode(argv[3])) {
            puts("Invalid mode. Defaulting to 'octet'");
            *mode = "octet";
        } else {
            *mode = str_tolower(argv[3]);
        }
    }
}
