#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include "consts.h"

// this function xors all the bits of the packet and returns True if it is 0
bool parity_check(uint8_t *buffer, size_t size)
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
void parse_packet(packet *pkt, uint8_t *buffer, size_t bytes_recvd)
{
    pkt->seq = (uint16_t)((buffer[0] << 8) | (buffer[1] & 0xFF));
    pkt->ack = (uint16_t)((buffer[2] << 8) | (buffer[3] & 0xFF));
    pkt->length = (uint16_t)((buffer[4] << 8) | (buffer[5] & 0xFF));
    pkt->win = (uint16_t)((buffer[6] << 8) | (buffer[7] & 0xFF));
    pkt->flags = (uint16_t)((buffer[9] << 8) | (buffer[8] & 0xFF)); // flags is not big endian
    pkt->unused = (uint16_t)((buffer[10] << 8) | (buffer[11] & 0xFF));

    // 12 is size of the metadata
    for (size_t i = 12; i < bytes_recvd; i++)
    {
        pkt->payload[i - 12] = buffer[i];
    }
}

// Main function of transport layer; never quits
int listen_loop(int sockfd, struct sockaddr_in addr, int type)
{

    if (type == SERVER)
    {
        uint8_t buffer[1024] = {0};
        socklen_t clientsize = sizeof(addr);
        int client_connected = 1; // just from project 0


        packet pkt;


        while (true)
        {
            // Receive from socket
            int bytes_recvd = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &clientsize);

            if (bytes_recvd <= 0 && client_connected == 0)
                continue;

            if (bytes_recvd > 0)
            {
                parse_packet(&pkt, buffer, bytes_recvd);
                // write to stdout
                write(STDOUT_FILENO, buffer, bytes_recvd);
            }

            // Read from stdin
            int bytes_inputted = read(STDIN_FILENO, buffer, 1024);

            // If data, send to socket
            if (bytes_inputted > 0)
            {
                int did_send = sendto(sockfd, buffer, bytes_inputted, 0, (struct sockaddr *)&addr, clientsize);
                if (did_send < 0)
                    return errno;
            }
        }
    }

    if (type == CLIENT)
    {
        uint8_t buffer[1024] = {0};
        socklen_t clientsize = sizeof(addr);
        int client_connected = 1; // just from project 0

        fprintf(stderr, "init\n");

        while (true)
        {
            // Receive from socket
            int bytes_recvd = recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr *)&addr, &clientsize);

            if (bytes_recvd <= 0 && client_connected == 0)
                continue;

            // If data, client is connected and write to stdout
            if (bytes_recvd > 0)
            {
                // write to stdout
                write(STDOUT_FILENO, buffer, bytes_recvd);
            }

            // Read from stdin
            int bytes_sent = read(STDIN_FILENO, buffer, 1024);

            // If data, send to socket
            if (bytes_sent > 0)
            {
                int did_send = sendto(sockfd, buffer, bytes_sent, 0, (struct sockaddr *)&addr, sizeof(addr));
                if (did_send < 0)
                    return errno;
            }
        }
    }

    return 0;
}
