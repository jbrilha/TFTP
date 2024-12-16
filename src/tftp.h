#ifndef TFTP_H
#define TFTP_H

#include "common.h"
#include <stdint.h>

#define BLOCK_SIZE 512

enum opcode { RRQ = 1, WRQ, DATA, ACK, ERROR };
enum errcode {
    UNDEFINED,
    FILE_NOT_FOUND,
    ACCESS_VIOLATION,
    DISK_FULL,
    ILLEGAL_OP,
    UKNOWN_TID,
    FILE_EXISTS,
    NO_SUCH_USER
};

typedef union {
    uint16_t opcode;
    struct {
        uint16_t opcode;
        uint8_t filename[BLOCK_SIZE];
        uint8_t fn_term; /* filename terminator byte */
        uint8_t mode[32];
        uint8_t mode_term; /* mode terminator byte */
    } req;

    struct {
        uint16_t opcode;
        uint16_t block_nr;
        uint8_t data[BLOCK_SIZE];
    } data;

    struct {
        uint16_t opcode;
        uint16_t block_nr;
    } ack;

    struct {
        uint16_t opcode;
        uint16_t error_code;
        uint8_t error_str[BLOCK_SIZE];
    } error;

} tftp_pkt;

ssize_t tftp_recv(int s, tftp_pkt *pkt, int flags, struct sockaddr *addr,
                  socklen_t *len);
ssize_t tftp_send(int s, tftp_pkt *pkt, struct sockaddr *addr, socklen_t len);

ssize_t tftp_send_req(int s, uint16_t opcode, char *filename, char *mode,
                      struct sockaddr *addr, socklen_t len);

ssize_t tftp_send_data(int s, uint16_t block_nr, uint8_t *data, size_t data_len,
                       struct sockaddr *addr, socklen_t len);

ssize_t tftp_send_ack(int s, uint16_t block_nr, struct sockaddr *addr,
                      socklen_t len);

ssize_t tftp_send_error(int s, uint16_t error_code, char *error_str,
                        struct sockaddr *addr, socklen_t len);

const char *opcode_to_str(uint16_t opcode);

#endif /* TFTP_H */
