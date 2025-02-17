#pragma once

#include <stdint.h>
#include <unistd.h>

// function to do parity check
bool parity_check(uint8_t *buffer, size_t size);

// fills in pkt with buffer information
void parse_packet(packet* pkt, uint8_t *buffer, size_t bytes_recvd);

// Main function of transport layer; never quits
void listen_loop(int sockfd, struct sockaddr_in addr, int type, int init_seq, int next_expected);
