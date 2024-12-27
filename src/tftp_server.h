#ifndef TFTP_SERVER_H
#define TFTP_SERVER_H

#include "tftp.h"

void recv_reqs(int sock);
void handle_client(tftp_pkt *pkt, struct sockaddr *addr, socklen_t slen);

#endif /* TFTP_SERVER_H */
