#include "tftp.h"

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

int validate_mode(char *mode) {
    return !strcasecmp(mode, "netascii") || !strcasecmp(mode, "octet");
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

FILE *open_file(const char *filename, const char *mode, uint16_t *err_code,
                const char **err_str) {

    FILE *fd = fopen(filename, mode);
    if (fd == NULL) {
        perror("fd");
        handle_file_error(err_code, err_str);
        printf("Could not open file '%s' in mode '%s': [%d] %s\n", filename,
               mode, *err_code, *err_str);
    }

    return fd;
}

ssize_t write_to_file(int sock, tftp_pkt *pkt, struct sockaddr *addr,
                      socklen_t slen, FILE *fd) {
    bool should_close = false;
    size_t dlen;
    ssize_t r;
    char filename[BLOCK_SIZE];
    strncpy(filename, (char *)pkt->req.filename, sizeof(filename) - 1);
    uint16_t expected_block_nr = 1;
    uint16_t received_block_nr;

    struct timeval tv = {.tv_sec = TIMEOUT, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    bool retry;
    u_int8_t retries;
    while (!should_close) {
        retries = 0;

        do {
            retry = false;
            if ((r = tftp_recv_pkt(sock, pkt, 0, addr, &slen)) < 0) {
                if (errno == EAGAIN && ++retries < MAX_RETRIES) {
                    retry = true;
                    continue;
                }
                return r;
            } else {
                dlen = r;
            }

            dlen -= 4;
            if (dlen < BLOCK_SIZE) {
                should_close = true;
            }

            switch (ntohs(pkt->opcode)) {
            case DATA:
                received_block_nr = ntohs(pkt->data.block_nr);
                if (received_block_nr < expected_block_nr) {
                    retry = false;
                } else if (received_block_nr == expected_block_nr) {
                    if (fwrite(pkt->data.data, 1, dlen, fd) != dlen) {
                        perror("fwrite failed");
                        return -1;
                    }
                    printf("WRITE DATA — bnr:%u / %zu\n",
                           ntohs(pkt->data.block_nr), dlen);
                    fflush(fd);
                    expected_block_nr++;
                    retry = false;
                } else {
                    retry = true;
                    printf("bad bnr: %d expected %d\n",
                           ntohs(pkt->data.block_nr), expected_block_nr);
                    break;
                }

                if ((r = tftp_send_ack(sock, pkt, received_block_nr, addr,
                                       slen)) < 0) {
                    return r;
                }
                printf("Sent ack: %d\n", received_block_nr);

                break;
            case ACK: // this case should never happen
                printf("WRITE ACK: %d\n", ntohs(pkt->ack.block_nr));
                break;
            case ERROR:
                printf("WRITE ERROR: %s\n", (char *)pkt->error.error_str);
                should_close = true;
                return -1;
            default:
                printf("WRITE %d shit\n", ntohs(pkt->opcode));
                return -1;
            }
        } while (retry && retries < MAX_RETRIES);
    }

    return r;
}

ssize_t read_from_file(int sock, tftp_pkt *pkt, struct sockaddr *addr,
                       socklen_t slen, FILE *fd) {
    ssize_t r;

    uint8_t data[BLOCK_SIZE];
    size_t dlen;
    uint16_t block_nr = 1, ack_nr;
    bool should_close = false;
    bool resend;

    struct timeval tv = {.tv_sec = TIMEOUT, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    u_int8_t retries;
    while (!should_close) {
        retries = 0;

        dlen = fread(data, 1, BLOCK_SIZE, fd);
        if (dlen < BLOCK_SIZE) {
            should_close = true;
        }

        do {
            resend = false;

            printf("Sending block %d size %zu\n", block_nr, dlen);
            if ((r = tftp_send_data(sock, pkt, block_nr, data, dlen, addr,
                                    slen)) < 0) {
                return r;
            }

            if ((r = tftp_recv_pkt(sock, pkt, 0, addr, &slen)) < 0) {
                if (errno == EAGAIN && ++retries < MAX_RETRIES) {
                    resend = true;
                    continue;
                }
                return r;
            }

            switch (ntohs(pkt->opcode)) {
            case ACK:
                ack_nr = ntohs(pkt->ack.block_nr);
                // printf("READ ACK: %d\n", ack_nr);
                if (ack_nr >= block_nr) {
                    block_nr++;
                    // puts("good ack");
                    retries = 0;
                    resend = false;
                } else {
                    printf("bad ack: %d expected %d\n", ack_nr, block_nr);
                    if (++retries < MAX_RETRIES) {
                        resend = true;
                    }
                }
                break;
            case ERROR:
                printf("READ ERROR: %s\n", (char *)pkt->error.error_str);
                should_close = true;
                resend = false;
                break;
            default:
                resend = true;
                retries++;
                printf("READ %d shit\n", ntohs(pkt->opcode));
            }
        } while (resend && retries < MAX_RETRIES);
        printf("\n");

        if (retries >= MAX_RETRIES) {
            perror("Max retries exceeded");
            return -ETIMEDOUT;
        }
    }

    printf("Sent %d blocks of data.\n", block_nr - 1);

    return r;
}

ssize_t tftp_send_req(int sock, tftp_pkt *pkt, uint16_t opcode, char *filename,
                      char *mode, struct sockaddr *addr, socklen_t slen) {
    memset(pkt, 0, sizeof(*pkt));

    pkt->opcode = htons(opcode);

    ssize_t flen = strlen(filename);
    memcpy(pkt->req.filename, filename, flen);
    pkt->req.fn_term = 0;

    ssize_t mlen = strlen(mode);
    memcpy(pkt->req.mode, mode, mlen);
    pkt->req.mode_term = 0;

    return tftp_send_pkt(sock, pkt, flen + mlen + 4, addr, slen);
}

ssize_t tftp_send_data(int sock, tftp_pkt *pkt, uint16_t block_nr,
                       uint8_t *data, size_t dlen, struct sockaddr *addr,
                       socklen_t slen) {
    memset(pkt, 0, sizeof(*pkt));

    pkt->opcode = htons(DATA);

    pkt->data.block_nr = htons(block_nr);
    memcpy(pkt->data.data, data, dlen);

    return tftp_send_pkt(sock, pkt, dlen + 4, addr, slen);
}

ssize_t tftp_send_ack(int sock, tftp_pkt *pkt, uint16_t bnr,
                      struct sockaddr *addr, socklen_t slen) {
    memset(pkt, 0, sizeof(*pkt));

    pkt->opcode = htons(ACK);

    ssize_t alen = 4;
    pkt->ack.block_nr = htons(bnr);

    return tftp_send_pkt(sock, pkt, alen, addr, slen);
}

ssize_t tftp_send_error(int sock, tftp_pkt *pkt, uint16_t error_code,
                        const char *error_str, struct sockaddr *addr,
                        socklen_t slen) {
    memset(pkt, 0, sizeof(*pkt));

    pkt->opcode = htons(ERROR);

    pkt->error.error_code = htons(error_code);

    ssize_t elen = strlen(error_str);
    memcpy(pkt->error.error_str, error_str, elen);

    return tftp_send_pkt(sock, pkt, elen + 4, addr, slen);
}

ssize_t tftp_send_pkt(int sock, tftp_pkt *pkt, ssize_t dlen,
                      struct sockaddr *addr, socklen_t slen) {
    ssize_t n = sendto(sock, pkt, dlen, 0, addr, slen);

    if (n < 0) {
        char error_msg[50];
        snprintf(error_msg, sizeof(error_msg), "Failed to send %s packet",
                 opcode_to_str(ntohs(pkt->opcode)));
        perror(error_msg);
    }

    return n;
}

ssize_t tftp_recv_pkt(int sock, tftp_pkt *pkt, int flags, struct sockaddr *addr,
                      socklen_t *slen) {
    ssize_t n = recvfrom(sock, pkt, sizeof(*pkt), flags, addr, slen);
    if (n < 0) {
        perror("Failed to receive packet");
    }

    return n;
}
