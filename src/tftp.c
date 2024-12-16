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
                      struct sockaddr *addr, socklen_t slen) {
    tftp_pkt r_pkt;
    r_pkt.opcode = htons(opcode);

    ssize_t flen = strlen(filename);
    memcpy(r_pkt.req.filename, filename, flen);
    r_pkt.req.fn_term = 0;

    ssize_t mlen = strlen(mode);
    memcpy(r_pkt.req.mode, mode, mlen);
    r_pkt.req.mode_term = 0;

    return tftp_send(s, &r_pkt, flen + mlen + 4, addr, slen);
}

ssize_t tftp_send_data(int s, uint16_t block_nr, uint8_t *data, size_t dlen,
                       struct sockaddr *addr, socklen_t slen) {
    tftp_pkt d_pkt;
    d_pkt.opcode = htons(DATA);

    d_pkt.data.block_nr = htons(block_nr);
    memcpy(d_pkt.data.data, data, dlen);

    return tftp_send(s, &d_pkt, dlen + 4, addr, slen);
}

ssize_t tftp_send_ack(int s, uint16_t block_nr, struct sockaddr *addr,
                      socklen_t slen) {
    tftp_pkt a_pkt;
    a_pkt.opcode = htons(ACK);

    ssize_t alen = 4;
    a_pkt.ack.block_nr = htons(block_nr);

    return tftp_send(s, &a_pkt, alen, addr, slen);
}

ssize_t tftp_send_error(int s, uint16_t error_code, char *error_str,
                        struct sockaddr *addr, socklen_t slen) {
    tftp_pkt e_pkt;
    e_pkt.opcode = htons(ERROR);

    e_pkt.error.error_code = htons(error_code);

    ssize_t elen = strlen(error_str);
    memcpy(e_pkt.error.error_str, error_str, elen);

    return tftp_send(s, &e_pkt, elen + 4, addr, slen);
}

ssize_t tftp_send(int s, tftp_pkt *pkt, ssize_t dlen, struct sockaddr *addr,
                  socklen_t slen) {
    ssize_t n = sendto(s, pkt, dlen, 0, addr, slen);

    if (n < 0) {
        char error_msg[50];
        snprintf(error_msg, sizeof(error_msg), "Failed to send %s packet",
                 opcode_to_str(ntohs(pkt->opcode)));
        perror(error_msg);
    }

    return n;
}

ssize_t tftp_recv(int s, tftp_pkt *pkt, int flags, struct sockaddr *addr,
                  socklen_t *slen) {
    ssize_t n = recvfrom(s, pkt, sizeof(*pkt), flags, addr, slen);
    if (n < 0) {
        perror("Failed to receive msg\n");
    }

    return n;
}
