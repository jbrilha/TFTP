#include "tftp.h"
#include <stdint.h>
#include <sys/_types/_ssize_t.h>

const char *opcode_to_str(uint16_t opcode) {
    switch (opcode) {
    case RRQ:
        return "RRQ";
    case WRQ:
        return "WRQ";
    case DATA:
        return "DATA";
    case ACK:
        return "ACK";
    case ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

ssize_t tftp_send_req(int s, uint16_t opcode, char *filename, char *mode,
                      struct sockaddr *addr, socklen_t len) {
    tftp_pkt r_pkt;
    r_pkt.opcode = htons(opcode);
    memcpy(r_pkt.req.filename, filename, strlen(filename));
    r_pkt.req.fn_term = 0;
    memcpy(r_pkt.req.mode, mode, strlen(mode));
    r_pkt.req.mode_term = 0;

    return tftp_send(s, &r_pkt, addr, len);
}

ssize_t tftp_send_data(int s, uint16_t block_nr, uint8_t *data, size_t data_len,
                       struct sockaddr *addr, socklen_t len) {
    tftp_pkt d_pkt;
    d_pkt.opcode = htons(DATA);
    d_pkt.data.block_nr = htons(block_nr);
    memcpy(d_pkt.data.data, data, data_len);

    return tftp_send(s, &d_pkt, addr, len);
}

ssize_t tftp_send_ack(int s, uint16_t block_nr, struct sockaddr *addr,
                      socklen_t len) {
    tftp_pkt a_pkt;
    a_pkt.opcode = htons(ACK);
    a_pkt.ack.block_nr = htons(block_nr);

    return tftp_send(s, &a_pkt, addr, len);
}

ssize_t tftp_send_error(int s, uint16_t error_code, char *error_str,
                        struct sockaddr *addr, socklen_t len) {
    tftp_pkt e_pkt;
    e_pkt.opcode = htons(ERROR);
    e_pkt.error.error_code = htons(error_code);
    memcpy(e_pkt.error.error_str, error_str, strlen(error_str));

    return tftp_send(s, &e_pkt, addr, len);
}

ssize_t tftp_send(int s, tftp_pkt *pkt, struct sockaddr *addr, socklen_t len) {
    ssize_t n = sendto(s, pkt, sizeof(*pkt), 0, addr, len);

    if (n < 0) {
        char error_msg[50];
        snprintf(error_msg, sizeof(error_msg), "Failed to send %s packet",
                 opcode_to_str(ntohs(pkt->opcode)));
        perror(error_msg);
    }

    return n;
}

ssize_t tftp_recv(int s, tftp_pkt *pkt, int flags, struct sockaddr *addr,
                  socklen_t *len) {
    ssize_t n = recvfrom(s, pkt, sizeof(*pkt), flags, addr, len);
    if (n < 0) {
        perror("Failed to receive msg\n");
    }

    return n;
}
