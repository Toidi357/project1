#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include "consts.h"
#include "helper.h"
#include <string.h>
#include <vector>
#include <map>
#include <chrono>
#include <unordered_set>

// timer to track last global ack
std::chrono::steady_clock::time_point last_ack_time;
const std::chrono::milliseconds timeout_duration(1000);

// Function to retransmit the first packet in packets_inflight
void retransmit_first_packet(int sockfd, struct sockaddr_in addr, std::vector<PacketInfo> &packets_inflight)
{
    if (!packets_inflight.empty())
    {
        PacketInfo &first_packet = packets_inflight.front();
        sendto(sockfd, first_packet.packet_data, first_packet.packet_size, 0,
               (struct sockaddr *)&addr, sizeof(addr));

        better_fprintf(stderr, "Retransmitting packet with SEQ: %d\n", first_packet.seq);
    }
}

// Main function of transport layer; never quits
int listen_loop(int sockfd, struct sockaddr_in addr, int init_seq, int next_expected, bool init_ack)
{

    uint8_t buffer[1024] = {0};
    socklen_t clientsize = sizeof(addr);
    int client_connected = 1; // just from project 0

    packet pkt;
    int current_seq = init_seq;

    bool create_ack = init_ack;

    // used for flow control
    int MAX_INFLIGHT = 20;                     // initially set max to 1 MSS
    std::vector<PacketInfo> packets_inflight; // contains the SEQ numbers of those in flight
    std::vector<PacketInfo> recv_buffer;      // contains SEQ numbers received not ACKed out yet

    // use in case of multiple retransmits
    std::unordered_set<int> received_packets;
    DupACKs dupacks;

    // use for retransmits
    last_ack_time = std::chrono::steady_clock::now();

    fprintf(stderr, "Expecting %d\n", next_expected);

    while (true)
    {
        // if 1 sec is up, retransmit first packet
        auto now = std::chrono::steady_clock::now();
        if (now - last_ack_time > timeout_duration)
        {
            retransmit_first_packet(sockfd, addr, packets_inflight);

            // Reset the timer
            last_ack_time = now;
        }

        // Receive from socket
        int bytes_recvd = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &clientsize);

        if (bytes_recvd <= 0 && client_connected == 0)
            continue;

        if (bytes_recvd > 0 && parity_check(buffer, bytes_recvd) == true)
        {
            parse_packet(&pkt, buffer, bytes_recvd);

            if (received_packets.count(pkt.seq) != 0) // this seq has already been seen
            {
                // create and send simple ACK
                int sent_bytes = create_and_send(sockfd, addr, buffer, 0, next_expected, MAX_INFLIGHT * MAX_PAYLOAD, true, 0);
                better_fprintf(stderr, "Already seen SEQ %d, Sent SEQ: %d ACK %d LEN: %d\n", pkt.seq, 0, next_expected, sent_bytes);

                create_ack = false;
            }
            else
            {
                // empty ACKs are SEQ 0
                if (pkt.seq != 0)
                    received_packets.insert(pkt.seq);

                // if this is an ACK packet, remove corresponding from sending buffer
                if (((pkt.flags >> 1) & 1) == 1)
                {
                    int highest_acked = pkt.ack - 1;
                    better_fprintf(stderr, "Received ACK: %d SEQ: %d LEN: %d\n", pkt.ack, pkt.seq, pkt.length);
                    arr_remove(packets_inflight, highest_acked);

                    // reset timer
                    last_ack_time = std::chrono::steady_clock::now();

                    // check if 3 dup acks, 2nd condition is to ensure that we haven't finished sending
                    if (dupacks.add(pkt.ack) && packets_inflight.size() != 0)
                    {
                        better_fprintf(stderr, "3 DUP ACKs received, retransmitting first packet in buffer\n");
                        retransmit_first_packet(sockfd, addr, packets_inflight);
                    }
                }

                // If packet is just a raw ACK, no need to do anything as it has SEQ 0
                if (pkt.length == 0)
                {
                    create_ack = false;
                }
                else
                {
                    create_ack = true;

                    // if this packet we get is the next in order, we dont buffer and straight print
                    if (pkt.seq == next_expected)
                    {
                        // write out contents of that packet
                        write(STDOUT_FILENO, pkt.payload, pkt.length);

                        // need to scan our recv_buffer for the highest in-order packet received,
                        // set next_expected to that one + 1, then clear the vector accordingly

                        if (recv_buffer.size() == 0) // if there's nothing in the buffer, then straight up increment and send ack
                        {
                            next_expected = pkt.seq + 1;
                        }
                        else if (recv_buffer.front().seq != pkt.seq + 1) // if the next in the buffer is not the one right after, that means we're still missing one
                        {
                            next_expected = pkt.seq + 1;
                        }
                        else
                        {
                            int highest_in_order_seq = arr_find_inorder(recv_buffer);

                            // write out everything we can from recv_buffer before deleting them
                            better_fprintf(stderr, "Writing out packets up to SEQ: %d\n", highest_in_order_seq);
                            int i = 0;
                            while (true)
                            {
                                PacketInfo pki = recv_buffer[i];
                                write(STDOUT_FILENO, pki.packet_data + PACKET_HEADER_SIZE, pki.packet_size - PACKET_HEADER_SIZE);
                                if (pki.seq == highest_in_order_seq)
                                    break;
                                i++;
                            }
                            arr_remove(recv_buffer, highest_in_order_seq);

                            next_expected = highest_in_order_seq + 1;
                        }
                    }
                    else
                    {
                        // otherwise, we buffer and do not update our next_expected (this will trigger a dup ack send)
                        PacketInfo pti;
                        pti.seq = pkt.seq;
                        pti.packet_size = bytes_recvd;
                        memcpy(pti.packet_data, buffer, bytes_recvd);
                        arr_insert(recv_buffer, pti);
                        better_fprintf(stderr, "Packet SEQ: %d buffered, expecting %d\n", pti.seq, next_expected);
                    }
                }
            }
        }

        // Read from stdin if we can
        if ((int)packets_inflight.size() < MAX_INFLIGHT)
        {
            int bytes_read = read(STDIN_FILENO, buffer, MAX_PAYLOAD);

            // 3 cases: data + no ack, data + yes ack, no data + yes ack
            if (create_ack == false && bytes_read > 0) // case data and no need to ack
            {
                int sent_bytes = create_and_send(sockfd, addr, buffer, current_seq, next_expected, MAX_INFLIGHT * MAX_PAYLOAD, false, bytes_read);
                better_fprintf(stderr, "Sent SEQ: %d ACK %d LEN: %d\n", current_seq, next_expected, sent_bytes - PACKET_HEADER_SIZE);

                PacketInfo pti;
                pti.seq = current_seq;
                pti.packet_size = sent_bytes;
                memcpy(pti.packet_data, buffer, sent_bytes);
                arr_insert(packets_inflight, pti); // this data packet is now in flight

                current_seq++; // increment the SEQ
            }
            else if (create_ack == true && bytes_read > 0) // case need to ack and data
            {
                // create and send
                int sent_bytes = create_and_send(sockfd, addr, buffer, current_seq, next_expected, MAX_INFLIGHT * MAX_PAYLOAD, true, bytes_read);
                better_fprintf(stderr, "Sent SEQ: %d ACK %d LEN: %d\n", current_seq, next_expected, sent_bytes - PACKET_HEADER_SIZE);

                PacketInfo pti;
                pti.seq = current_seq;
                pti.packet_size = sent_bytes;
                memcpy(pti.packet_data, buffer, sent_bytes);
                arr_insert(packets_inflight, pti); // this data packet is now in flight

                current_seq++; // increment our SEQ
                create_ack = false;
            }
            else if (create_ack == true) // case need to ack and no data
            {
                // create an ACK and send it out                   empty ACKs have SEQ 0
                int sent_bytes = create_and_send(sockfd, addr, buffer, 0, next_expected, MAX_INFLIGHT * MAX_PAYLOAD, true, 0);
                better_fprintf(stderr, "Sent SEQ: %d ACK %d LEN: %d\n", 0, next_expected, sent_bytes - PACKET_HEADER_SIZE);

                create_ack = false;
            }
        }
        else if (create_ack == true) // if we've hit max inflight packets but have an ACK to send out, just do it
        {
            // create an ACK and send it out                   empty ACKs have SEQ 0
            int sent_bytes = create_and_send(sockfd, addr, buffer, 0, next_expected, MAX_INFLIGHT * MAX_PAYLOAD, true, 0);
            better_fprintf(stderr, "Sent SEQ: %d ACK %d LEN: %d\n", 0, next_expected, sent_bytes - PACKET_HEADER_SIZE);

            // next_expected++;
            create_ack = false;
        }
    }

    return 0;
}
