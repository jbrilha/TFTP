#ifndef TFTP_CLIENT_H
#define TFTP_CLIENT_H

#include "tftp.h"

void parse_args(int argc, char **argv, uint16_t *opcode, char **filename,
                char **mode);

#endif /* TFTP_CLIENT_H */
