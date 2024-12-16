#include "tftp_client.h"

int main(int argc, char **argv) {
    int s = create_ipv4_socket();
    struct sockaddr_in s_addr_in = init_ipv4_addr(PORT, false);
    // struct sockaddr_in6 s_addr_in = init_ipv6_addr(PORT, false);
    socklen_t socklen = sizeof(s_addr_in);
    struct sockaddr *s_addr = (struct sockaddr *)&s_addr_in;

    // unnecessary for udp
    /* int c = connect(sock, (struct sockaddr *)&server_addr,
    sizeof(server_addr)); if (c < 0) { perror("Failed to connect socket\n");
        exit(EXIT_FAILURE);
    } */

    uint16_t op = RRQ;
    char *filename = "file.txt";
    char *mode = "octet";
    if (tftp_send_req(s, op, filename, mode, s_addr, socklen) < 0) {
        exit(EXIT_FAILURE);
    }

    char *data = "dataAAAAAA";
    uint16_t blocknr = 1;
    if (tftp_send_data(s, blocknr, data, s_addr, socklen) < 0) {
        exit(EXIT_FAILURE);
    }

    char *err = "errorrrrrr";
    uint16_t errcode = 15;
    if (tftp_send_error(s, errcode, err, s_addr, socklen) < 0) {
        exit(EXIT_FAILURE);
    }

    uint16_t ack = 5;
    if (tftp_send_ack(s, ack, s_addr, socklen) < 0) {
        exit(EXIT_FAILURE);
    }

    close(s);
}
