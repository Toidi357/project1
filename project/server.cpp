#include "consts.h"
#include "transport.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>


// fills in the char* buffer with the packet and returns packet length
int create_syn_ack_packet(uint8_t* buffer, int seq){
    packet pkt;
    pkt.seq = htons(rand() % 1001); // random
    pkt.ack = htons(seq + 1);    // set to previous SEQ + 1
    pkt.length = htons(0); // length of payload
    pkt.win = htons(1012); // constant set
    pkt.flags = SYN | ACK;
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
    if (argc < 2)
    {
        fprintf(stderr, "Usage: server <port>\n");
        exit(1);
    }

    // seed our randomness generator
    srand(time(NULL));

    /* Create sockets */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    // use IPv4  use UDP

    /* Construct our address */
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET; // use IPv4
    server_addr.sin_addr.s_addr =
        INADDR_ANY; // accept all connections
                    // same as inet_addr("0.0.0.0")
                    // "Address string to network bytes"
    // Set receiving port
    int PORT = atoi(argv[1]);
    server_addr.sin_port = htons(PORT); // Big endian

    /* Let operating system know about our config */
    bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    struct sockaddr_in client_addr; // Same information, but about client
    socklen_t s = sizeof(struct sockaddr_in);
    uint8_t buffer[1024] = {0};

    // Wait for client connection
    fprintf(stderr, "%s\n", "[DEBUG]: Listening for SYN packets");
    int bytes_recvd = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                               (struct sockaddr *)&client_addr, &s);
    // upon getting a packet, check its SEQ number and SYN flag, note everything is already in big endian order
    packet syn_pkt;
    if (parity_check(buffer, bytes_recvd) == false)
        return 0; // drop packet
    parse_packet(&syn_pkt, buffer, bytes_recvd);
    print_diag(&syn_pkt);

    // send out SYN ACK
    int syn_ack_size = create_syn_ack_packet(buffer, syn_pkt.seq);
    int did_send = sendto(sockfd, buffer, syn_ack_size, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
    fprintf(stderr, "%s %d %s\n", "[DEBUG] Sent SYN ACK packet of", did_send, "bytes");

    // wait for ACK
    bytes_recvd = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &s);
    packet ack_pkt;
    if (parity_check(buffer, bytes_recvd) == false)
        return 0; // drop packet
    parse_packet(&ack_pkt, buffer, bytes_recvd);
    print_diag(&ack_pkt);

    // nonblocking
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);


    listen_loop(sockfd, client_addr, SERVER);

    return 0;
}
