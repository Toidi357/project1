#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include "consts.h"
#include <string.h>
#include <vector>

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

    size_t payload_size = bytes_recvd - PACKET_HEADER_SIZE;
    if (payload_size > MAX_PAYLOAD) {
        fprintf(stderr, "Payload size exceeds maximum allowed size. Truncating.\n");
        payload_size = MAX_PAYLOAD;
    }

    memcpy(pkt->payload, buffer + PACKET_HEADER_SIZE, payload_size);
}

// creates a packet with given info into the specified buffer, returns packet size
// will set the ACK flag is ackflag param set to true
int create_packet(uint8_t *buffer, int seq, int ack, int win, bool ackflag, int payload_size)
{
    size_t packet_size = PACKET_HEADER_SIZE + payload_size;
    uint8_t temp_buffer[packet_size];
    packet *pkt = (packet *)temp_buffer;

    pkt->seq = htons(seq);
    pkt->ack = htons(ack);
    pkt->length = htons(payload_size);
    pkt->win = htons(win);
    pkt->flags = ackflag ? ACK : 0;
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

// Main function of transport layer; never quits
int listen_loop(int sockfd, struct sockaddr_in addr, int type, int init_seq, int next_expected)
{

    if (type == SERVER)
    {
        uint8_t buffer[1024] = {0};
        socklen_t clientsize = sizeof(addr);
        int client_connected = 1; // just from project 0

        packet pkt;
        int current_packet = init_seq;
        int expected_packet = next_expected;
        fprintf(stderr, "Initial: %d %d\n", current_packet, expected_packet);

        while (true)
        {
            // Receive from socket
            int bytes_recvd = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &clientsize);

            if (bytes_recvd <= 0 && client_connected == 0)
                continue;

            if (bytes_recvd > 0)
            {
                fprintf(stderr, "bytesrecvd: %d\n", bytes_recvd);
 
                parse_packet(&pkt, buffer, bytes_recvd);

                // write to stdout
                write(STDOUT_FILENO, pkt.payload, bytes_recvd - PACKET_HEADER_SIZE);
                int pkt_size = create_packet(buffer, current_packet, expected_packet + 1, 1012, true, 0);

                int did_send = sendto(sockfd, buffer, pkt_size, 0, (struct sockaddr *)&addr, sizeof(addr));
                if (did_send < 0)
                    return errno;

                current_packet++;
                expected_packet++;
            }

            // // Read from stdin
            // int bytes_inputted = read(STDIN_FILENO, buffer, MAX_PAYLOAD);

            // // If data, send to socket
            // if (bytes_inputted > 0)
            // {
            //     int did_send = sendto(sockfd, buffer, bytes_inputted, 0, (struct sockaddr *)&addr, clientsize);
            //     if (did_send < 0)
            //         return errno;
            // }
        }
    }

    if (type == CLIENT)
    {
        uint8_t buffer[1024] = {0};
        socklen_t clientsize = sizeof(addr);
        int client_connected = 1; // just from project 0

        packet pkt;
        int current_packet = init_seq;
        int expected_packet = next_expected;

        // used for flow control
        int MAX_INFLIGHT = 2;              // initially set max to 1 MSS
        std::vector<int> packets_inflight; // contains the SEQ numbers of those in flight

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
                fprintf(stderr, "bytesrecvd: %d\n", bytes_recvd);
                parse_packet(&pkt, buffer, bytes_recvd);

                // if ack packet
                if ((pkt.flags >> 1) & 1)
                {
                    // figure out the ack number
                    int highest_acked = pkt.ack - 1;
                    arr_remove(packets_inflight, highest_acked);
                }
                // write to stdout
                write(STDOUT_FILENO, pkt.payload, bytes_recvd - PACKET_HEADER_SIZE);
            }

            // Read from stdin if we can
            if ((int)packets_inflight.size() < MAX_INFLIGHT)
            {
                int bytes_sent = read(STDIN_FILENO, buffer, MAX_PAYLOAD);

                // If data, send to socket
                if (bytes_sent > 0)
                {
                    int pkt_size = create_packet(buffer, current_packet, next_expected, 1012, false, bytes_sent);
                    fprintf(stderr, "bytessent: %d\n", pkt_size);

                    int did_send = sendto(sockfd, buffer, pkt_size, 0, (struct sockaddr *)&addr, sizeof(addr));
                    if (did_send < 0)
                        return errno;

                    arr_insert(packets_inflight, current_packet); // number of packets unacked
                    current_packet++;                             // increment the SEQ
                }
            }
        }
    }

    return 0;
}
