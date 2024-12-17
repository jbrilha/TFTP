#include "tftp.h"

static const char *error_strs[] = {
    [UNDEFINED] = "Not defined, see error message.",
    [FILE_NOT_FOUND] = "File not found.",
    [ACCESS_VIOLATION] = "Access violation.",
    [DISK_FULL] = "Disk full or allocation exceeded.",
    [ILLEGAL_OP] = "Illegal TFTP operation.",
    [UKNOWN_TID] = "Unknown transfer ID.",
    [FILE_EXISTS] = "File already exists.",
    [NO_SUCH_USER] = "No such user."};

const char *errcode_to_str(enum errcode code) {
    if (code >= 0 && code <= NO_SUCH_USER) {
        return error_strs[code];
    }
    return "Unknown error";
}

static const char *opcode_strs[] = {[RRQ] = "RRQ",
                                    [WRQ] = "WRQ",
                                    [DATA] = "DATA",
                                    [ACK] = "ACK",
                                    [ERROR] = "ERROR"};

const char *opcode_to_str(enum opcode code) {
    if (code >= 1 && code <= 5) {
        return opcode_strs[code];
    }
    return "UNKNOWN";
}

uint16_t str_to_opcode(char *op_str) {
    if (!strcasecmp(op_str, "RRQ")) {
        return RRQ;
    } else if (!strcasecmp(op_str, "WRQ")) {
        return WRQ;
    } else if (!strcasecmp(op_str, "DATA")) {
        return DATA;
    } else if (!strcasecmp(op_str, "ACK")) {
        return ACK;
    } else if (!strcasecmp(op_str, "ERROR")) {
        return ERROR;
    }

    return 0;
}

void handle_file_error(uint16_t *err_code, const char **err_str) {
    switch (errno) {
    case (EEXIST):
        *err_str = error_strs[FILE_EXISTS];
        *err_code = FILE_EXISTS;
        return;
    case (ENOENT):
        *err_str = error_strs[FILE_NOT_FOUND];
        *err_code = FILE_NOT_FOUND;
        return;
    case (EACCES):
        *err_str = error_strs[ACCESS_VIOLATION];
        *err_code = ACCESS_VIOLATION;
        return;
    case (ENOSPC):
        *err_str = error_strs[DISK_FULL];
        *err_code = DISK_FULL;
        return;
    default:
        *err_str = error_strs[UNDEFINED];
        *err_code = UNDEFINED;
        return;
    }
}

ssize_t handle_write(int s, tftp_pkt *pkt, struct sockaddr *s_addr,
                     socklen_t slen) {
    FILE *fd;
    bool should_close = false, file_is_open = false;
    ssize_t dlen;
    puts("1");
    char filename[BLOCK_SIZE];
    strncpy(filename, (char *)pkt->req.filename, sizeof(filename) - 1);

    puts("2");
    while (!should_close) {
    puts("3");
        if ((dlen = tftp_recv(s, pkt, 0, s_addr, &slen)) < 0) {
            exit(EXIT_FAILURE);
        }
    puts("4");

        dlen -= 4;
        if (dlen < BLOCK_SIZE) {
            should_close = true;
        }
    puts("5");

        uint16_t ack;
        switch (ntohs(pkt->opcode)) {
        case DATA:
    puts("6");
            if (!file_is_open) {
                fd = fopen(filename, "w");
                if (fd == NULL) {
                    perror("handle_write");
                    const char *err_str;
                    uint16_t err_code;
                    handle_file_error(&err_code, &err_str);
    puts("7");
                    return tftp_send_error(s, pkt, err_code, err_str, s_addr,
                                           slen);
                }
                file_is_open = true;
            }

            ack = ntohs(pkt->data.block_nr);
            if (fwrite(pkt->data.data, 1, dlen, fd) != dlen) {
                perror("fwrite failed");
                exit(EXIT_FAILURE);
            }
            fflush(fd);
            if (tftp_send_ack(s, pkt, ack, s_addr, slen) < 0) {
                exit(EXIT_FAILURE);
            }
            printf("WRITE DATA — bnr:%u / %d\n", ntohs(pkt->data.block_nr),
                   dlen);
            break;
        case ACK:
            printf("WRITE ACK: %d\n", ntohs(pkt->ack.block_nr));
            break;
        case ERROR:
            printf("WRITE ERROR: %s\n", (char *)pkt->error.error_str);
            break;
        default:
            printf("WRITE %d shit\n", ntohs(pkt->opcode));
        }
    }
    puts("8");

    if (file_is_open)
        fclose(fd);
}

ssize_t handle_read(int s, tftp_pkt *pkt, struct sockaddr *addr,
                    socklen_t slen) {
    FILE *fd;
    char *filename = (char *)pkt->req.filename;
    ssize_t r;

    fd = fopen(filename, "r");
    if (fd == NULL) {
        perror("handle_write");
        const char *err_str;
        uint16_t err_code;
        handle_file_error(&err_code, &err_str);
        tftp_send_error(s, pkt, err_code, err_str, addr, slen);
        return -1;
    }

    uint8_t data[BLOCK_SIZE];
    size_t dlen;
    uint16_t block_nr = 0;
    bool should_close = false;

    while (!should_close) {
        dlen = fread(data, 1, BLOCK_SIZE, fd);
        printf("Sending %zu bytes...\n", dlen);
        block_nr++;
        if (dlen < BLOCK_SIZE) {
            should_close = true;
        }

        if ((r = tftp_send_data(s, pkt, block_nr, data, dlen, addr, slen)) <
            0) {
            return r;
        }

        if ((r = tftp_recv(s, pkt, 0, addr, &slen)) < 0) {
            return r;
        }

        switch (ntohs(pkt->opcode)) {
        case ACK:
            printf("READ ACK: %d\n", ntohs(pkt->ack.block_nr));
            continue;
        case ERROR:
            printf("READ ERROR: %s\n", (char *)pkt->error.error_str);
            break;
        default:
            printf("READ %d shit\n", ntohs(pkt->opcode));
        }
    }

    printf("Sent %d blocks of data.\n", block_nr);
    fclose(fd);

    return r;
}

ssize_t tftp_send_req(int s, tftp_pkt *pkt, uint16_t opcode, char *filename,
                      char *mode, struct sockaddr *addr, socklen_t slen) {
    memset(pkt, 0, sizeof(*pkt));

    pkt->opcode = htons(opcode);

    ssize_t flen = strlen(filename);
    memcpy(pkt->req.filename, filename, flen);
    pkt->req.fn_term = 0;

    ssize_t mlen = strlen(mode);
    memcpy(pkt->req.mode, mode, mlen);
    pkt->req.mode_term = 0;

    return tftp_send(s, pkt, flen + mlen + 4, addr, slen);
}

ssize_t tftp_send_data(int s, tftp_pkt *pkt, uint16_t block_nr, uint8_t *data,
                       size_t dlen, struct sockaddr *addr, socklen_t slen) {
    memset(pkt, 0, sizeof(*pkt));

    pkt->opcode = htons(DATA);

    pkt->data.block_nr = htons(block_nr);
    memcpy(pkt->data.data, data, dlen);

    return tftp_send(s, pkt, dlen + 4, addr, slen);
}

ssize_t tftp_send_ack(int s, tftp_pkt *pkt, uint16_t block_nr,
                      struct sockaddr *addr, socklen_t slen) {
    memset(pkt, 0, sizeof(*pkt));

    pkt->opcode = htons(ACK);

    ssize_t alen = 4;
    pkt->ack.block_nr = htons(block_nr);

    return tftp_send(s, pkt, alen, addr, slen);
}

ssize_t tftp_send_error(int s, tftp_pkt *pkt, uint16_t error_code,
                        const char *error_str, struct sockaddr *addr,
                        socklen_t slen) {
    memset(pkt, 0, sizeof(*pkt));

    pkt->opcode = htons(ERROR);

    pkt->error.error_code = htons(error_code);

    ssize_t elen = strlen(error_str);
    memcpy(pkt->error.error_str, error_str, elen);

    return tftp_send(s, pkt, elen + 4, addr, slen);
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

int validate_mode(char *mode) {
    return !strcasecmp(mode, "netascii") || !strcasecmp(mode, "octet");
}
