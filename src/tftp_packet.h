#ifndef TFTP_PACKET_H
#define TFTP_PACKET_H

#include "common.h"

enum opcode { RRQ = 1, WRQ, DATA, ACK, ERROR };

typedef union {
    uint16_t opcode;
    struct {
        uint16_t opcode;
        uint8_t filename[512];
        uint8_t fn_term; /* filename terminator byte */
        uint8_t mode[32];
        uint8_t mode_term; /* mode terminator byte */
    } req;

    struct {
        uint16_t opcode;
        uint16_t block_nr;
        uint8_t data[512];
    } data;

    struct {
        uint16_t opcode;
        uint16_t block_nr;
    } ack;

    struct {
        uint16_t opcode;
        uint16_t error_code;
        uint8_t error_str[512];
    } error;

} tftp_pkt;

ssize_t tftp_send(int s, tftp_pkt pkt, struct sockaddr *addr, socklen_t len);

ssize_t tftp_send_req(int s, uint16_t opcode, char *filename, char *mode,
                      struct sockaddr *addr, socklen_t len);

ssize_t tftp_send_data(int s, uint16_t block_nr, char *data,
                       struct sockaddr *addr, socklen_t len);

ssize_t tftp_send_ack(int s, uint16_t block_nr, struct sockaddr *addr,
                      socklen_t len);

ssize_t tftp_send_error(int s, uint16_t error_code, char *error_str,
                        struct sockaddr *addr, socklen_t len);

const char *opcode_to_str(uint16_t opcode);

#endif /* TFTP_PACKET_H */
