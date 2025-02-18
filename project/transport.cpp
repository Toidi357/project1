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
void retransmit_first_packet(int sockfd, struct sockaddr_in addr, std::vector<PacketInfo> &packets_inflight) {
    if (!packets_inflight.empty()) {
        PacketInfo &first_packet = packets_inflight.front();
        sendto(sockfd, first_packet.packet_data, first_packet.packet_size, 0,
               (struct sockaddr *)&addr, sizeof(addr));

        fprintf(stderr, "Retransmitting packet with SEQ: %d\n", first_packet.seq);
    }
}


// Main function of transport layer; never quits
int listen_loop(int sockfd, struct sockaddr_in addr, int init_seq, int next_expected)
{

    uint8_t buffer[1024] = {0};
    socklen_t clientsize = sizeof(addr);
    int client_connected = 1; // just from project 0

    packet pkt;
    int current_seq = init_seq;
    int highest_seq_received = next_expected;

    bool create_ack = false;

    // used for flow control
    int MAX_INFLIGHT = 1;              // initially set max to 1 MSS
    std::vector<PacketInfo> packets_inflight; // contains the SEQ numbers of those in flight
    std::vector<int> recv_buffer;      // contains SEQ numbers received not ACKed out yet

    // use in case of multiple retransmits
    std::unordered_set<int> received_packets;
    
    // use for retransmits
    last_ack_time = std::chrono::steady_clock::now();

    while (true)
    {
        // if 1 sec is up, retransmit first packet
        auto now = std::chrono::steady_clock::now();
        if (now - last_ack_time > timeout_duration) {
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
                // create and send ACK
                int sent_bytes = create_and_send(sockfd, addr, buffer, current_seq, highest_seq_received, MAX_INFLIGHT * MAX_PAYLOAD, true, 0);
                fprintf(stderr, "Already seen SEQ %d, Sent SEQ: %d ACK %d LEN: %d\n", pkt.seq, current_seq, highest_seq_received, sent_bytes);

                current_seq++;

                create_ack = false;
            }
            else
            {
                received_packets.insert(pkt.seq);

                // if this is an ACK packet, remove corresponding from sending buffer
                if (((pkt.flags >> 1) & 1) == 1)
                {
                    int highest_acked = pkt.ack - 1;
                    fprintf(stderr, "Received ACK: %d SEQ: %d\n", pkt.ack, pkt.seq);
                    arr_remove(packets_inflight, highest_acked);
                    highest_seq_received = pkt.seq + 1;

                    // reset timer
                    last_ack_time = std::chrono::steady_clock::now();
                }

                // If packet is just a raw ACK, no need to worry
                if (pkt.length == 0)
                {
                    create_ack = false;
                }
                else
                {
                    create_ack = true;
                    highest_seq_received = pkt.seq + 1;
                    arr_insert_std(recv_buffer, pkt.seq);
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
                int sent_bytes = create_and_send(sockfd, addr, buffer, current_seq, highest_seq_received, MAX_INFLIGHT * MAX_PAYLOAD, false, bytes_read);
                fprintf(stderr, "Sent SEQ: %d ACK %d LEN: %d\n", current_seq, highest_seq_received, sent_bytes);

                PacketInfo pti;
                pti.seq = current_seq;
                pti.packet_size = sent_bytes;
                memcpy(pti.packet_data, buffer, sent_bytes);
                arr_insert(packets_inflight, pti); // this data packet is now in flight

                current_seq++; // increment the SEQ
            }
            else if (create_ack == true && bytes_read > 0) // case need to ack and data
            {
                // get packet in buffer to ACK
                int to_ack = recv_buffer.back();

                // create and send
                int sent_bytes = create_and_send(sockfd, addr, buffer, current_seq, highest_seq_received, MAX_INFLIGHT * MAX_PAYLOAD, true, bytes_read);
                fprintf(stderr, "Sent SEQ: %d ACK %d LEN: %d\n", current_seq, highest_seq_received, sent_bytes);

                PacketInfo pti;
                pti.seq = current_seq;
                pti.packet_size = sent_bytes;
                memcpy(pti.packet_data, buffer, sent_bytes);
                arr_insert(packets_inflight, pti); // this data packet is now in flight

                arr_remove_std(recv_buffer, to_ack); // remove it from our unacked buffers

                current_seq++; // increment our SEQ
                highest_seq_received = to_ack + 1;
                create_ack = false;

                // write out contents of that packet
                write(STDOUT_FILENO, pkt.payload, pkt.length);
            }
            else if (create_ack == true) // case need to ack and no data
            {
                // get packet in buffer to ACK
                int to_ack = recv_buffer.back();

                // create an ACK and send it out
                int sent_bytes = create_and_send(sockfd, addr, buffer, current_seq, to_ack + 1, MAX_INFLIGHT * MAX_PAYLOAD, true, 0);
                fprintf(stderr, "Sent SEQ: %d ACK %d LEN: %d\n", current_seq, to_ack + 1, sent_bytes);

                // remove it from our unacked buffers
                arr_remove_std(recv_buffer, to_ack);
                current_seq++; // increment our SEQ
                highest_seq_received = to_ack + 1;
                create_ack = false;

                // write out contents of that packet
                write(STDOUT_FILENO, pkt.payload, pkt.length);
            }
        }
        else if (create_ack == true) // if we've hit max inflight packets but have an ACK to send out, just do it
        {
            // get packet in buffer to ACK
            int to_ack = recv_buffer.back();

            // create an ACK and send it out
            int sent_bytes = create_and_send(sockfd, addr, buffer, current_seq, to_ack + 1, MAX_INFLIGHT * MAX_PAYLOAD, true, 0);
            fprintf(stderr, "Sent SEQ: %d ACK %d LEN: %d\n", current_seq, to_ack + 1, sent_bytes);

            // remove it from our unacked buffers
            arr_remove_std(recv_buffer, to_ack);
            current_seq++; // increment our SEQ
            highest_seq_received = to_ack + 1;
            create_ack = false;

            // write out contents of that packet
            write(STDOUT_FILENO, pkt.payload, pkt.length);
        }
    }

    return 0;
}
