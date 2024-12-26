#ifndef TFTP_H
#define TFTP_H

#include "common.h"
#include "utils.h"

#define BLOCK_SIZE 512
#define MODE_SIZE 10

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
        uint8_t filename[BLOCK_SIZE - MODE_SIZE];
        uint8_t fn_term; /* filename terminator byte */
        uint8_t mode[MODE_SIZE];
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
                  socklen_t *slen);
ssize_t tftp_send(int s, tftp_pkt *pkt, ssize_t dlen, struct sockaddr *addr,
                  socklen_t slen);

ssize_t tftp_send_req(int s, tftp_pkt *pkt, uint16_t opcode, char *filename,
                      char *mode, struct sockaddr *addr, socklen_t len);

ssize_t tftp_send_data(int s, tftp_pkt *pkt, uint16_t block_nr, uint8_t *data,
                       size_t data_len, struct sockaddr *addr, socklen_t len);

ssize_t tftp_send_ack(int s, tftp_pkt *pkt, uint16_t block_nr,
                      struct sockaddr *addr, socklen_t len);

ssize_t tftp_send_error(int s, tftp_pkt *pkt, uint16_t error_code,
                        const char *error_str, struct sockaddr *addr,
                        socklen_t len);

ssize_t handle_write(int s, tftp_pkt *pkt, struct sockaddr *s_addr,
                     socklen_t slen, FILE *fd);
ssize_t handle_read(int s, tftp_pkt *pkt, struct sockaddr *addr, socklen_t slen,
                    FILE *fd);
void handle_file_error(uint16_t *err_code, const char **err_str);
FILE *open_file(const char *filename, const char *mode, uint16_t *err_code,
                const char **err_str);

const char *errcode_to_str(enum errcode code);
const char *opcode_to_str(enum opcode code);

uint16_t str_to_opcode(char *op_str);

int validate_mode(char *mode);

#endif /* TFTP_H */
