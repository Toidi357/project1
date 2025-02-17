#include "consts.h"
#include "transport.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>


// fills in the char* buffer with the packet and returns packet length
int create_syn_packet(uint8_t* buffer, int payload_size)
{
    size_t packet_size = PACKET_HEADER_SIZE + payload_size;
    uint8_t temp_buffer[packet_size];
    packet *pkt = (packet *)temp_buffer;

    pkt->seq = htons(rand() % 1001);
    pkt->ack = htons(0);    // not used
    pkt->length = htons(0); // length of payload
    pkt->win = htons(1012); // constant set
    pkt->flags = SYN;
    pkt->unused = htons(0);
    memcpy(pkt->payload, buffer, payload_size);

    // check parity
    if (parity_check(temp_buffer, packet_size) == false)
    {
        pkt->flags |= PARITY;
    }

    memcpy(buffer, temp_buffer, packet_size);

    return packet_size;
}

int create_ack_packet(uint8_t* buffer, int seq, int ack, int payload_size) {
    size_t packet_size = PACKET_HEADER_SIZE + payload_size;
    uint8_t temp_buffer[packet_size];
    packet *pkt = (packet *)temp_buffer;

    pkt->seq = htons(ack);
    pkt->ack = htons(seq + 1);
    pkt->length = htons(0);
    pkt->win = htons(1012);
    pkt->flags = ACK;
    pkt->unused = htons(0);
    memcpy(pkt->payload, buffer, payload_size);

    // check parity
    if (parity_check(temp_buffer, packet_size) == false)
    {
        pkt->flags |= PARITY;
    }

    memcpy(buffer, temp_buffer, packet_size);

    return packet_size;
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

    // Create sockets    use IPv4  use UDP
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    

    /* Construct server address */
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    server_addr.sin_family = AF_INET; // use IPv4
    // Only supports localhost as a hostname, but that's all we'll test on
    const char *addr = strcmp(argv[1], "localhost") == 0 ? "127.0.0.1" : argv[1];
    server_addr.sin_addr.s_addr = inet_addr(addr);
    // Set sending port
    int PORT = atoi(argv[2]);
    server_addr.sin_port = htons(PORT); // Big endian


    // send syn packet of 3 way handshake
    uint8_t buffer[1024] = {0};
    int syn_pkt_size = create_syn_packet(buffer, 0);
    int did_send = sendto(sockfd, buffer, syn_pkt_size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    fprintf(stderr, "%s %d %s\n", "[DEBUG] Sent SYN packet of", did_send, "bytes");


    // wait for SYN ACK
    int bytes_recvd = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, &addr_len);
    packet syn_ack;
    if (parity_check(buffer, bytes_recvd) == false)
        return 0; // drop packet
    parse_packet(&syn_ack, buffer, bytes_recvd);
    print_diag(&syn_ack);

    // send ACK
    int ack_pkt_size = create_ack_packet(buffer, syn_ack.seq, syn_ack.ack, 0);
    did_send = sendto(sockfd, buffer, ack_pkt_size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    fprintf(stderr, "[DEBUG] Sent ACK packet of %d bytes\n", did_send);

    // nonblocking
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);


    listen_loop(sockfd, server_addr, CLIENT, syn_ack.ack + 1, syn_ack.seq + 1);

    return 0;
}
