#include "consts.h"
#include "io.h"
#include "transport.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>


// fills in the char* buffer with the packet and returns packet length
int create_syn_packet(char* buffer)
{
    packet pkt;
    pkt.seq = htons(rand() % 1001);
    pkt.ack = htons(0);    // not used
    pkt.length = htons(0); // length of payload
    pkt.win = htons(1012); // constant set
    pkt.flags = SYN;
    pkt.unused = htons(0);
    memcpy(buffer, &pkt, sizeof(pkt));
    // check parity
    if (parity_check(buffer, sizeof(pkt)) == false)
    {
        pkt.flags |= PARITY;
        memcpy(buffer, &pkt, sizeof(pkt));
    }

    return sizeof(pkt);
}

int create_ack_packet(char* buffer, int seq) {
    packet pkt;
    pkt.seq = htons(0); // send 0 for now
    pkt.ack = htons(seq + 1);
    pkt.length = htons(0);
    pkt.win = htons(1012);
    pkt.flags = ACK;
    pkt.unused = htons(0);
    memcpy(buffer, &pkt, sizeof(pkt));
    // check parity
    if (parity_check(buffer, sizeof(pkt)) == false)
    {
        pkt.flags |= PARITY;
        memcpy(buffer, &pkt, sizeof(pkt));
    }

    return sizeof(pkt);
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: client <hostname> <port> \n");
        exit(1);
    }

    // seed our randomness generator
    srand(time(NULL));

    /* Create sockets */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    // use IPv4  use UDP

    /* Construct server address */
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    server_addr.sin_family = AF_INET; // use IPv4
    // Only supports localhost as a hostname, but that's all we'll test on
    char *addr = strcmp(argv[1], "localhost") == 0 ? "127.0.0.1" : argv[1];
    server_addr.sin_addr.s_addr = inet_addr(addr);
    // Set sending port
    int PORT = atoi(argv[2]);
    server_addr.sin_port = htons(PORT); // Big endian


    // send syn packet of 3 way handshake
    char buffer[1024] = {0};
    int syn_pkt_size = create_syn_packet(buffer);
    int did_send = sendto(sockfd, buffer, syn_pkt_size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    fprintf(stderr, "%s %d %s\n", "[DEBUG] Sent SYN packet of", did_send, "bytes");


    // wait for SYN ACK
    int bytes_recvd = recvfrom(sockfd, buffer, sizeof(buffer), MSG_PEEK, (struct sockaddr *)&server_addr, &addr_len);
    packet syn_ack;
    if (parity_check(buffer, bytes_recvd) == false)
        return 0; // drop packet
    parse_packet(&syn_ack, buffer, bytes_recvd);
    print_diag(&syn_ack);

    // send ACK
    int ack_pkt_size = create_ack_packet(buffer, syn_ack.seq);
    did_send = sendto(sockfd, buffer, ack_pkt_size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    fprintf(stderr, "[DEBUG] Sent ACK packet of %d bytes\n", did_send);


    return 0;
    init_io();
    listen_loop(sockfd, &server_addr, CLIENT, input_io, output_io);

    return 0;
}
