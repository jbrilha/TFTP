#ifndef TFTP_SERVER_H
#define TFTP_SERVER_H

#include "tftp.h"

void recv_reqs(int sock);

void handle_client(tftp_pkt *pkt, struct sockaddr *addr, socklen_t slen);
void handle_RRQ(int sock, tftp_pkt *pkt, struct sockaddr *addr, socklen_t slen);
void handle_WRQ(int sock, tftp_pkt *pkt, struct sockaddr *addr, socklen_t slen);

int bind_socket(int sock);

#endif /* TFTP_SERVER_H */
