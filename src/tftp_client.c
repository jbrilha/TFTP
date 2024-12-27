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

    const char *err_str;
    uint16_t err_code;
    const char *f_mode = (opcode == RRQ) ? "w" : "r";
    FILE *fd = open_file(filename, f_mode, &err_code, &err_str);
    if (fd == NULL) {
        close(sock);
        exit(EXIT_FAILURE);
    }

    if (send_req(sock, opcode, filename, mode, fd) < 0) {
        cleanup_and_exit(fd, sock, filename, opcode == RRQ);
    }

    close(sock);
    exit(EXIT_SUCCESS);
}

ssize_t send_req(int sock, u_int16_t opcode, char *filename, char *mode,
                 FILE *fd) {
    struct sockaddr_in s_addr_in = init_ipv4_addr(PORT, false);
    socklen_t slen = sizeof(s_addr_in);
    struct sockaddr *s_addr = (struct sockaddr *)&s_addr_in;

    tftp_pkt pkt;
    if (tftp_send_req(sock, &pkt, opcode, filename, mode, s_addr, slen) < 0) {
        return -1;
    }

    switch (opcode) {
    case RRQ:
        return handle_RRQ(sock, &pkt, s_addr, slen, fd);
        break;
    case WRQ:
        return handle_WRQ(sock, &pkt, s_addr, slen, fd);
        break;
    }

    return -1;
}

ssize_t handle_RRQ(int sock, tftp_pkt *pkt, struct sockaddr *addr,
                   socklen_t slen, FILE *fd) {
    if (write_to_file(sock, pkt, addr, slen, fd) < 0) {
        fprintf(stderr, "RRQ failed with ERROR: %s\n",
                errcode_to_str(ntohs(pkt->error.error_code)));
        return -1;
    }

    fclose(fd);
    return 1;
}

ssize_t handle_WRQ(int sock, tftp_pkt *pkt, struct sockaddr *addr,
                   socklen_t slen, FILE *fd) {
    if ((tftp_recv_pkt(sock, pkt, 0, addr, &slen) < 0) ||
        (htons(pkt->opcode) != ACK && htons(pkt->opcode) != ERROR)) {
        perror("Expected ACK or ERROR");
        return -1;
    }

    u_int16_t ack_or_err = htons(pkt->opcode);
    if (ack_or_err == ERROR) {
        fprintf(stderr, "WRQ failed: %s\n",
                errcode_to_str(ntohs(pkt->error.error_code)));
        return -1;
    }

    u_int16_t ack_block_nr = ntohs(pkt->ack.block_nr);
    if (ack_or_err == ACK && ack_block_nr != 0) {
        fprintf(stderr, "WRQ failed: Expected ACK block_nr 0, received %d\n",
                ack_block_nr);
        return -1;
    }

    if (read_from_file(sock, pkt, addr, slen, fd) < 0) {
        fprintf(stderr, "WRQ failed with ERROR: %s\n",
                errcode_to_str(ntohs(pkt->error.error_code)));
        return -1;
    }

    fclose(fd);
    return 1;
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
