#include "tftp_server.h"

int main(int argc, char **argv) {
    int sock;
    if ((sock = create_ipv4_socket()) < 0) {
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in s_addr_in = init_ipv4_addr(PORT, true);
    struct sockaddr *s_addr = (struct sockaddr *)&s_addr_in;

    if (bind(sock, s_addr, sizeof(s_addr_in)) < 0) {
        perror("Failed to bind socket");
        close(sock);
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, SIG_IGN);

    recv_reqs(sock);

    close(sock);
    exit(EXIT_SUCCESS);
}

void recv_reqs(int sock) {
    struct sockaddr_in c_addr_in;
    struct sockaddr *c_addr = (struct sockaddr *)&c_addr_in;

    socklen_t slen;
    pid_t pid;

    while (1) {
        tftp_pkt pkt;
        slen = sizeof(c_addr_in);

        if (tftp_recv_pkt(sock, &pkt, 0, c_addr, &slen) < 0) {
            perror("Invalid request packet");
            continue;
        }

        pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            continue;
        } else if (pid == 0) {
            printf("pid %d\n", pid);
            close(sock); // child processes can close server socket
            handle_client(&pkt, c_addr, slen);
            exit(EXIT_SUCCESS);
        }
    }
}

void handle_client(tftp_pkt *pkt, struct sockaddr *addr, socklen_t slen) {
    int sock;
    if ((sock = create_ipv4_socket()) < 0) {
        return;
    }

    const char *err_str;
    uint16_t err_code;

    FILE *fd;
    uint16_t opcode = ntohs(pkt->opcode);
    switch (opcode) {
    case RRQ: {
        char *filename = (char *)pkt->req.filename;
        fd = open_file(filename, "r", &err_code, &err_str);
        if (fd == NULL) {
            tftp_send_error(sock, pkt, err_code, err_str, addr, slen);
            close(sock);
            return;
        }
        printf("RRQ for: %s\n", filename);
        if (read_from_file(sock, pkt, addr, slen, fd) < 0) {
            fprintf(stderr, "RRQ failed with ERROR: %s\n",
                    errcode_to_str(ntohs(pkt->error.error_code)));
        }
    } break;
    case WRQ: {
        char *filename = (char *)pkt->req.filename;
        fd = open_file(filename, "wx", &err_code, &err_str);
        if (fd == NULL) {
            tftp_send_error(sock, pkt, err_code, err_str, addr, slen);
            close(sock);
            return;
        }

        if (tftp_send_ack(sock, pkt, 0, addr, slen) < 0) {
            perror("Failed to send ACK");
            break;
        }

        if (write_to_file(sock, pkt, addr, slen, fd) < 0) {
            // TODO error.error_code might not always be set
            fprintf(stderr, "WRQ failed with ERROR: %s\n",
                    errcode_to_str(ntohs(pkt->error.error_code)));
            remove(filename);
        }
    } break;
    default:
        fprintf(stderr, "Invalid opcode: <%d>\nExpected RRQ or WRQ.\n", opcode);

        close(sock);
        return;
    }

    fclose(fd);
    close(sock);
}
