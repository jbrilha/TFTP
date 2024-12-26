#ifndef TFTP_CLIENT_H
#define TFTP_CLIENT_H

#include "tftp.h"

void cleanup_and_exit(FILE *fd, int socket, const char *filename,
                      bool remove_file);
void parse_args(int argc, char **argv, uint16_t *opcode, char **filename,
                char **mode);

#endif /* TFTP_CLIENT_H */
