#include "tftp_client.h"
#include "tftp.h"

int main(int argc, char **argv) {
    char *filename, *mode;
    uint16_t opcode;
    parse_args(argc, argv, &opcode, &filename, &mode);

    if (!access(filename, F_OK) && opcode == RRQ) {
        char response[3];
        printf("File already exists. Overwrite? [y/N] ");
        if (fgets(response, sizeof(response), stdin) == NULL ||
            tolower(response[0]) != 'y') {
            puts("Terminaning client...");
            exit(EXIT_FAILURE);
        }
    }

    int s = create_ipv4_socket();
    struct sockaddr_in s_addr_in = init_ipv4_addr(PORT, false);
    socklen_t slen = sizeof(s_addr_in);
    struct sockaddr *s_addr = (struct sockaddr *)&s_addr_in;

    tftp_pkt pkt;
    const char *err_str;
    uint16_t err_code;
    const char *f_mode = (opcode == RRQ) ? "w" : "r";
    FILE *fd = open_file(filename, f_mode, &err_code, &err_str);
    if (fd == NULL) {
        close(s);
        exit(EXIT_FAILURE);
    }

    switch (opcode) {
    case RRQ:
        if (tftp_send_req(s, &pkt, opcode, filename, mode, s_addr, slen) < 0) {
            close(s);
            exit(EXIT_FAILURE);
        }
        if (handle_write(s, &pkt, s_addr, slen, fd) < 0) {
            printf("RRQ failed with ERROR: %s\n",
                   errcode_to_str(ntohs(pkt.error.error_code)));
            remove(filename);
        }
        break;
    case WRQ:
        }
        if (tftp_send_req(s, &pkt, opcode, filename, mode, s_addr, slen) < 0) {
            close(s);
            exit(EXIT_FAILURE);
        }

        if (tftp_recv(s, &pkt, 0, s_addr, &slen) < 0) {
            perror("Expected ACK");
        } else {
            u_int16_t ack_or_err = htons(pkt.opcode);
            if (ack_or_err == ACK && pkt.ack.block_nr == 0) {
                if (handle_read(s, &pkt, s_addr, slen, fd) < 0) {
                    printf("WRQ failed with ERROR: %s\n",
                           errcode_to_str(ntohs(pkt.error.error_code)));
                }
            } else if (ack_or_err == ERROR) {
                printf("WRQ failed: %s\n",
                       errcode_to_str(ntohs(pkt.error.error_code)));
                fclose(fd);
                close(s);
                exit(EXIT_FAILURE);
            } else {
                perror("WRQ failed");
                fclose(fd);
                close(s);
                exit(EXIT_FAILURE);
            }
            printf("%zd\n", htons(pkt.opcode));
        }
        break;
    default:
        break;
    }

    }

    fclose(fd);
    close(s);
    exit(EXIT_SUCCESS);
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
