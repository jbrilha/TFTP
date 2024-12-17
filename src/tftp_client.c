#include "tftp_client.h"

int main(int argc, char **argv) {
    char *filename, *mode;
    uint16_t opcode;
    parse_args(argc, argv, &opcode, &filename, &mode);
    // printf("%hu|%s|%s\n", opcode, filename, mode);

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

    if (!access(filename, F_OK) && opcode == RRQ) {
        char response[3];
        printf("File already exists. Overwrite? [y/N] ");
        if (fgets(response, sizeof(response), stdin) == NULL ||
            (response[0] != 'y' && response[0] != 'Y')) {
            puts("Terminaning client...");
            exit(EXIT_FAILURE);
        }
    }

    tftp_pkt pkt;
    if (tftp_send_req(s, &pkt, opcode, filename, mode, s_addr, slen) < 0) {
        exit(EXIT_FAILURE);
    }

    switch (opcode) {
    case RRQ:
        handle_write(s, &pkt, s_addr, slen);
        break;
    case WRQ:
        handle_read(s, &pkt, s_addr, slen);
    default:
        break;
    }

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
