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
    struct timeval tv = {.tv_sec = TIMEOUT, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char filename[BLOCK_SIZE];
    strncpy(filename, (char *)pkt->req.filename, sizeof(filename) - 1);

    u_int8_t recv_retries, ack_retries;
    uint16_t ack_block_nr, expected_block_nr = 1;
    size_t dlen;
    ssize_t r;

    bool retry, should_close = false;
    struct receiver_stats stats;
    init_stats(&stats);
    puts("Starting transfer");

    while (!should_close) {
        ack_retries = 0;

        do {
            retry = false;
            if ((r = tftp_recv_pkt(sock, pkt, 0, addr, &slen)) < 0) {
                if (errno == EAGAIN && ++recv_retries < MAX_RETRIES) {
                    retry = true;
                    fprintf(stderr, "\nFailed to receive packet [%d / %d]\n",
                            recv_retries, MAX_RETRIES);
                    continue;
                }
                const char *err_str = "Failed to receive packet";
                tftp_send_error(sock, pkt, UNDEFINED, err_str, addr, slen);
                return r;
            }
            recv_retries = 0;

            dlen = r - 4;
            if (dlen < BLOCK_SIZE) {
                should_close = true;
            }

            switch (ntohs(pkt->opcode)) {
            case DATA:
                ack_block_nr = ntohs(pkt->data.block_nr);
                if (ack_block_nr < expected_block_nr) {
                    retry = false;
                } else if (ack_block_nr == expected_block_nr) {
                    if (fwrite(pkt->data.data, 1, dlen, fd) != dlen ||
                        ferror(fd)) {
                        const char *err_str = "Failed to write to file";
                        perror(err_str);
                        tftp_send_error(sock, pkt, UNDEFINED, err_str, addr,
                                        slen);
                        return -1;
                    }
                    fflush(fd);
                    update_stats(&stats, dlen);
                    expected_block_nr++;
                    retry = false;
                } else {
                    if (++ack_retries < MAX_RETRIES) {
                        retry = true;
                    }
                    ack_block_nr = expected_block_nr - 1;
                    fprintf(
                        stderr,
                        "\nInvalid block_nr received: (%d) but expected (%d).\n"
                        "Resending ack %d [%d / %d]\n",
                        ntohs(pkt->data.block_nr), expected_block_nr,
                        ack_block_nr, recv_retries, MAX_RETRIES);
                }

                if ((r = tftp_send_ack(sock, pkt, ack_block_nr, addr, slen)) <
                    0) {
                    const char *err_str =
                        "Failed to send ACK, likely network issue";
                    tftp_send_error(sock, pkt, UNDEFINED, err_str, addr, slen);
                    return r;
                }
                break;
            case ERROR:
                fprintf(stderr, "WRITE ERROR: %s\n", (char *)pkt->error.error_str);
                return -1;
            default:
                fprintf(stderr, "WRITE invalid packet: %d\n", ntohs(pkt->opcode));
                const char *err_str = "Invalid packet received";
                tftp_send_error(sock, pkt, ILLEGAL_OP, err_str, addr, slen);
                return -EINVAL;
            }
        } while (retry && recv_retries < MAX_RETRIES &&
                 ack_retries < MAX_RETRIES);

        if (recv_retries >= MAX_RETRIES || ack_retries >= MAX_RETRIES) {
            const char *err_str = "Max retries exceeded, ending transmission";
            perror(err_str);
            tftp_send_error(sock, pkt, UNDEFINED, err_str, addr, slen);
            return -ETIMEDOUT;
        }
    }

    if (should_close) {
        show_receiver_stats(&stats);
    } else {
        puts("Transfer failed to complete");
    }

    return r;
}

ssize_t read_from_file(int sock, tftp_pkt *pkt, struct sockaddr *addr,
                       socklen_t slen, FILE *fd) {
    struct timeval tv = {.tv_sec = TIMEOUT, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t data[BLOCK_SIZE];
    u_int8_t recv_retries, ack_retries;
    uint16_t block_nr = 1, ack_nr;
    size_t dlen;
    ssize_t r;

    bool should_close = false;
    bool resend;

    struct sender_progress progress;
    fseek(fd, 0L, SEEK_END);
    size_t file_size = ftell(fd);
    fseek(fd, 0L, SEEK_SET);
    init_progress(&progress, file_size);
    puts("Starting transfer");

    while (!should_close) {
        ack_retries = 0;

        dlen = fread(data, 1, BLOCK_SIZE, fd);
        if (ferror(fd)) {
            const char *err_str = "Failed to read from file";
            perror(err_str);
            tftp_send_error(sock, pkt, UNDEFINED, err_str, addr, slen);
            return -1;
        }
        if (dlen < BLOCK_SIZE) {
            should_close = true;
        }

        do {
            resend = false;

            if ((r = tftp_send_data(sock, pkt, block_nr, data, dlen, addr,
                                    slen)) < 0) {
                const char *err_str =
                    "Failed to send data packet, likely network issue";
                tftp_send_error(sock, pkt, UNDEFINED, err_str, addr, slen);
                return r;
            }

            if ((r = tftp_recv_pkt(sock, pkt, 0, addr, &slen)) < 0) {
                if (errno == EAGAIN && ++recv_retries < MAX_RETRIES) {
                    fprintf(stderr,
                            "\nFailed to receive ACK, resending data [%d / %d]\n",
                            recv_retries, MAX_RETRIES);
                    resend = true;
                    continue;
                }
                const char *err_str = "Failed to receive packet";
                tftp_send_error(sock, pkt, UNDEFINED, err_str, addr, slen);
                return r;
            }
            recv_retries = 0;

            switch (ntohs(pkt->opcode)) {
            case ACK:
                ack_nr = ntohs(pkt->ack.block_nr);
                if (ack_nr >= block_nr) {
                    block_nr++;
                    update_progress(&progress, dlen);
                    resend = false;
                } else {
                    if (++ack_retries < MAX_RETRIES) {
                        resend = true;
                    }
                    fprintf(stderr,
                            "Invalid ACK received: (%d) but expected (%d).\n"
                            "Resending data [%d / %d]",
                            ack_nr, block_nr, ack_retries, MAX_RETRIES);
                }
                break;
            case ERROR:
                printf("READ ERROR: %s\n", (char *)pkt->error.error_str);
                return -1;
            default:
                fprintf(stderr, "READ invalid packet: %d", ntohs(pkt->opcode));
                const char *err_str = "Invalid packet received";
                tftp_send_error(sock, pkt, ILLEGAL_OP, err_str, addr, slen);
                return -EINVAL;
            }
        } while (resend && recv_retries < MAX_RETRIES &&
                 ack_retries < MAX_RETRIES);

        if (recv_retries >= MAX_RETRIES || ack_retries >= MAX_RETRIES) {
            const char *err_str = "Max retries exceeded, ending transmission";
            perror(err_str);
            tftp_send_error(sock, pkt, UNDEFINED, err_str, addr, slen);
            return -ETIMEDOUT;
        }
    }

    if (should_close) {
        show_sender_completion(&progress);
    } else {
        puts("Transfer failed to complete");
    }

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
    return recvfrom(sock, pkt, sizeof(*pkt), flags, addr, slen);
}
