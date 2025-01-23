#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include "consts.h"

// this function xors all the bits of the packet and returns True if it is 0
bool parity_check(char *buffer, size_t size)
{
    bool val = 0;

    for (size_t i = 0; i < size; i++)
    {
        // for every byte, we loop through the bits to xor
        for (int j = 0; j < 8; j++)
        {
            val ^= (buffer[i] >> j) & 1;
        }
    }

    return val == 0;
}

// fills in pkt with buffer information
void parse_packet(packet* pkt, char *buffer, size_t bytes_recvd)
{
    pkt->seq = (uint16_t)((buffer[0] << 8) | (buffer[1] & 0xFF));
    pkt->ack = (uint16_t)((buffer[2] << 8) | (buffer[3] & 0xFF));
    pkt->length = (uint16_t)((buffer[4] << 8) | (buffer[5] & 0xFF));
    pkt->win = (uint16_t)((buffer[6] << 8) | (buffer[7] & 0xFF));
    pkt->flags = (uint16_t)((buffer[9] << 8) | (buffer[8] & 0xFF)); // flags is not big endian
    pkt->unused = (uint16_t)((buffer[10] << 8) | (buffer[11] & 0xFF));

    // 12 is size of the metadata
    for (size_t i = 12; i < bytes_recvd; i++) {
        pkt->payload[i - 12] = buffer[i];
    }
}

// Main function of transport layer; never quits
void listen_loop(int sockfd, struct sockaddr_in *addr, int type,
                 ssize_t (*input_p)(uint8_t *, size_t),
                 void (*output_p)(uint8_t *, size_t))
{

    if (type == SERVER)
    {
        while (true)
        {
            
        }
    }
}
