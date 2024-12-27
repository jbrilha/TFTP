#include "tftp_client.h"

int main(int argc, char **argv) {
    char *filename, *mode;
    uint16_t opcode;
    parse_args(argc, argv, &opcode, &filename, &mode);

    if (!access(filename, F_OK) && opcode == RRQ) {
        char response[3];
        printf("File already exists. Overwrite? [y/N] ");
        if (fgets(response, sizeof(response), stdin) == NULL ||
            tolower(response[0]) != 'y') {
            puts("Terminating client...");
            exit(EXIT_FAILURE);
        }
    }

    int sock;
    if ((sock = create_ipv4_socket()) < 0) {
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in s_addr_in = init_ipv4_addr(PORT, false);
    socklen_t slen = sizeof(s_addr_in);
    struct sockaddr *s_addr = (struct sockaddr *)&s_addr_in;

    const char *err_str;
    uint16_t err_code;
    const char *f_mode = (opcode == RRQ) ? "w" : "r";
    FILE *fd = open_file(filename, f_mode, &err_code, &err_str);
    if (fd == NULL) {
        close(sock);
        exit(EXIT_FAILURE);
    }

    tftp_pkt pkt;
    if (tftp_send_req(sock, &pkt, opcode, filename, mode, s_addr, slen) < 0) {
        cleanup_and_exit(fd, sock, filename, opcode == RRQ);
    }

    switch (opcode) {
    case RRQ:
        if (write_to_file(sock, &pkt, s_addr, slen, fd) < 0) {
            fprintf(stderr, "RRQ failed with ERROR: %s\n",
                    errcode_to_str(ntohs(pkt.error.error_code)));
            cleanup_and_exit(fd, sock, filename, true);
        }
        break;
    case WRQ:
        if ((tftp_recv_pkt(sock, &pkt, 0, s_addr, &slen) < 0) ||
            (htons(pkt.opcode) != ACK && htons(pkt.opcode) != ERROR)) {
            perror("Expected ACK or ERROR");
            cleanup_and_exit(fd, sock, filename, false);
        }

        u_int16_t ack_or_err = htons(pkt.opcode);
        if (ack_or_err == ERROR) {
            fprintf(stderr, "WRQ failed: %s\n",
                    errcode_to_str(ntohs(pkt.error.error_code)));
            cleanup_and_exit(fd, sock, filename, false);
        }

        u_int16_t ack_block_nr = ntohs(pkt.ack.block_nr);
        if (ack_or_err == ACK && ack_block_nr != 0) {
            fprintf(stderr,
                    "WRQ failed: Expected ACK block_nr 0, received %d\n",
                    ack_block_nr);
            cleanup_and_exit(fd, sock, filename, false);
        }

        if (read_from_file(sock, &pkt, s_addr, slen, fd) < 0) {
            fprintf(stderr, "WRQ failed with ERROR: %s\n",
                    errcode_to_str(ntohs(pkt.error.error_code)));
            cleanup_and_exit(fd, sock, filename, false);
        }
        break;
    }

    fclose(fd);
    close(sock);
    exit(EXIT_SUCCESS);
}

void cleanup_and_exit(FILE *fd, int socket, const char *filename,
                      bool remove_file) {
    if (fd)
        fclose(fd);
    if (socket >= 0)
        close(socket);
    if (remove_file)
        remove(filename);
    exit(EXIT_FAILURE);
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
