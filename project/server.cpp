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
int create_syn_ack_packet(uint8_t* buffer, int seq, int payload_size){
    size_t packet_size = PACKET_HEADER_SIZE + payload_size;
    uint8_t temp_buffer[packet_size];
    packet *pkt = (packet *)temp_buffer;

    pkt->seq = htons(rand() % 1001); // random
    pkt->ack = htons(seq + 1);    // set to previous SEQ + 1
    pkt->length = htons(payload_size); // length of payload
    pkt->win = htons(1012); // constant set
    pkt->flags = SYN | ACK;
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

// creates a quick and simple ACK to the client's ACK in the 3-way
int create_simple_ack_packet(uint8_t* buffer, int ack){
    size_t packet_size = PACKET_HEADER_SIZE;
    uint8_t temp_buffer[packet_size];
    packet *pkt = (packet *)temp_buffer;

    pkt->seq = htons(0);
    pkt->ack = htons(ack);
    pkt->length = htons(0);
    pkt->win = htons(1012); // constant set
    pkt->flags = SYN | ACK;
    pkt->unused = htons(0);

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

    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

    uint8_t buffer[1024] = {0};

    // Wait for client connection
    fprintf(stderr, "%s\n", "[DEBUG] Listening for SYN packets");
    int bytes_recvd = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                               (struct sockaddr *)&client_addr, &s);
    // upon getting a packet, check its SEQ number and SYN flag, note everything is already in big endian order
    packet syn_pkt;
    parse_packet(&syn_pkt, buffer, bytes_recvd);
    print_diag(&syn_pkt);
    if (syn_pkt.length != 0) 
        write(STDOUT_FILENO, syn_pkt.payload, syn_pkt.length);

    // send out SYN ACK
    int bytes_sent = read(STDIN_FILENO, buffer, MAX_PAYLOAD); // workaround since packets of 3-way handshake can contain data
    if (bytes_sent < 0) bytes_sent = 0; // bullsh*t
    int syn_ack_size = create_syn_ack_packet(buffer, syn_pkt.seq, bytes_sent);
    int did_send = sendto(sockfd, buffer, syn_ack_size, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
    fprintf(stderr, "%s %d %s\n", "[DEBUG] Sent SYN ACK packet of", did_send, "bytes");

    // wait for ACK
    bytes_recvd = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &s);
    packet ack_pkt;
    parse_packet(&ack_pkt, buffer, bytes_recvd);
    print_diag(&ack_pkt);
    int expected = syn_pkt.seq + 1;
    if (ack_pkt.length != 0)
    {
        write(STDOUT_FILENO, ack_pkt.payload, ack_pkt.length);
        expected = ack_pkt.seq + 1;

        // need a quick ACK back
        bytes_sent = create_simple_ack_packet(buffer, ack_pkt.seq + 1);
        sendto(sockfd, buffer, bytes_sent, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
        fprintf(stderr, "[DEBUG] Sent simple ACK packet of %d bytes\n", bytes_sent);
    }

    // nonblocking
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);

    listen_loop(sockfd, client_addr, ack_pkt.ack, expected, false);

    return 0;
}
