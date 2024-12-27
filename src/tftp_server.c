#include "tftp_server.h"

int main(int argc, char **argv) {
    int sock;
    if ((sock = create_ipv4_socket()) < 0) {
        exit(EXIT_FAILURE);
    }

    if (bind_socket(sock)) {
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

    uint16_t opcode = ntohs(pkt->opcode);
    switch (opcode) {
    case RRQ:
        handle_RRQ(sock, pkt, addr, slen);
        break;
    case WRQ:
        handle_WRQ(sock, pkt, addr, slen);
        break;
    default:
        fprintf(stderr, "Invalid opcode: <%d>\nExpected RRQ or WRQ.\n", opcode);

        close(sock);
        return;
    }

    close(sock);
}

void handle_RRQ(int sock, tftp_pkt *pkt, struct sockaddr *addr,
                socklen_t slen) {
    const char *err_str;
    uint16_t err_code;

    char *filename = (char *)pkt->req.filename;
    printf("Received RRQ for: %s\n", filename);

    FILE *fd = open_file(filename, "r", &err_code, &err_str);
    if (fd == NULL) {
        tftp_send_error(sock, pkt, err_code, err_str, addr, slen);
        return;
    }

    if (read_from_file(sock, pkt, addr, slen, fd) < 0) {
        fprintf(stderr, "RRQ ERROR: %s\n", pkt->error.error_str);
    }

    fclose(fd);
}

void handle_WRQ(int sock, tftp_pkt *pkt, struct sockaddr *addr,
                socklen_t slen) {
    const char *err_str;
    uint16_t err_code;

    char *filename = (char *)pkt->req.filename;
    printf("Received WRQ for: %s\n", filename);

    FILE *fd = open_file(filename, "wx", &err_code, &err_str);
    if (fd == NULL) {
        tftp_send_error(sock, pkt, err_code, err_str, addr, slen);
        return;
    }

    if (tftp_send_ack(sock, pkt, 0, addr, slen) < 0) {
        perror("Failed to send ACK, likely network issue");
        fclose(fd);
        return;
    }

    if (write_to_file(sock, pkt, addr, slen, fd) < 0) {
        fprintf(stderr, "WRQ ERROR: %s\n", pkt->error.error_str);
        remove(filename);
    }

    fclose(fd);
}

int bind_socket(int sock) {
    struct sockaddr_in s_addr_in = init_ipv4_addr(PORT, true);
    struct sockaddr *s_addr = (struct sockaddr *)&s_addr_in;

    int b;
    if ((b = bind(sock, s_addr, sizeof(s_addr_in))) < 0) {
        perror("Failed to bind socket");
        close(sock);
    }

    return b;
}
