#ifndef TFTP_CLIENT_H
#define TFTP_CLIENT_H

#include "tftp.h"

ssize_t send_req(int sock, u_int16_t opcode, char *filename, char *mode,
                 FILE *fd);
ssize_t handle_RRQ(int sock, tftp_pkt *pkt, struct sockaddr *addr,
                   socklen_t slen, FILE *fd);
ssize_t handle_WRQ(int sock, tftp_pkt *pkt, struct sockaddr *addr,
                   socklen_t slen, FILE *fd);

void cleanup_and_exit(FILE *fd, int socket, const char *filename,
                      bool remove_file);
void parse_args(int argc, char **argv, uint16_t *opcode, char **filename,
                char **mode);

#endif /* TFTP_CLIENT_H */
