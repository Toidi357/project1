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

// Timer structure to track packet retransmissions
struct PacketTimer
{
    int seq;
    std::chrono::steady_clock::time_point sent_time;
    int retries;
    uint8_t packet_data[1024];
    size_t packet_size;
};

std::vector<PacketTimer> packet_timers;

const std::chrono::milliseconds timeout_duration(1000);

// Similar to arr_insert and arr_remove, we're keeping sorted order so need special function
void insert_timer(int seq, const uint8_t *packet_data, size_t packet_size)
{
    PacketTimer timer;
    timer.seq = seq;
    timer.sent_time = std::chrono::steady_clock::now();
    timer.retries = 69; // Number of retries
    memcpy(timer.packet_data, packet_data, packet_size);
    timer.packet_size = packet_size;

    // Find the correct position to insert the timer
    auto pos = std::lower_bound(packet_timers.begin(), packet_timers.end(), seq,
                                [](const PacketTimer &timer, int seq)
                                {
                                    return timer.seq < seq;
                                });
    packet_timers.insert(pos, timer);
}
void remove_timers_up_to(int seq)
{
    if (packet_timers.size() == 0)
        return;
    auto pos = std::lower_bound(packet_timers.begin(), packet_timers.end(), seq,
                                [](const PacketTimer &timer, int seq)
                                {
                                    return timer.seq < seq;
                                });
    if (pos != packet_timers.end() && pos->seq == seq)
    {
        packet_timers.erase(packet_timers.begin(), pos + 1);
    }
}

// Runs every while loop to handle timeout-based retransmissions
void check_timers(int sockfd, struct sockaddr_in addr)
{
    auto now = std::chrono::steady_clock::now();
    for (auto it = packet_timers.begin(); it != packet_timers.end();)
    {
        if (now - it->sent_time > timeout_duration)
        {
            if (it->retries > 0)
            {
                // Retransmit the packet
                sendto(sockfd, it->packet_data, it->packet_size, 0,
                       (struct sockaddr *)&addr, sizeof(addr));

                // Update the timer and retry count
                it->sent_time = now;
                it->retries--;
                ++it;
            }
            else
            {
                // Remove the packet if retries are exhausted
                it = packet_timers.erase(it);
            }
        }
        else
        {
            ++it;
        }
    }
}

// Main function of transport layer; never quits
int listen_loop(int sockfd, struct sockaddr_in addr, int init_seq, int next_expected)
{

    uint8_t buffer[1024] = {0};
    socklen_t clientsize = sizeof(addr);
    int client_connected = 1; // just from project 0

    packet pkt;
    int current_packet = init_seq;
    int expected_packet = next_expected;

    bool create_ack = false;

    // used for flow control
    int MAX_INFLIGHT = 1;              // initially set max to 1 MSS
    std::vector<int> packets_inflight; // contains the SEQ numbers of those in flight
    std::vector<int> recv_buffer;      // contains SEQ numbers received not ACKed out yet

    // use in case of multiple retransmits
    std::unordered_set<int> received_packets;
    

    while (true)
    {
        check_timers(sockfd, addr);

        // Receive from socket
        int bytes_recvd = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &clientsize);

        if (bytes_recvd <= 0 && client_connected == 0)
            continue;

        if (bytes_recvd > 0)
        {
            // drop packet if corrupted
            if (parity_check(buffer, bytes_recvd) == false)
            {
                fprintf(stderr, "Detect corrupt\n");
                continue;
            }

            parse_packet(&pkt, buffer, bytes_recvd);

            if (received_packets.count(pkt.seq) != 0) // this seq has already been seen
            {
                fprintf(stderr, "Already seen SEQ %d\n", pkt.seq);
                // create and send ACK
                int pkt_size = create_packet(buffer, current_packet, expected_packet, 1012, true, 0);
                sendto(sockfd, buffer, pkt_size, 0, (struct sockaddr *)&addr, sizeof(addr));
                current_packet++;

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
                    expected_packet = pkt.seq + 1;

                    remove_timers_up_to(highest_acked); // set up timer
                }

                // If packet is just a raw ACK, no need to worry
                if (pkt.length == 0)
                {
                    create_ack = false;
                }
                else
                {
                    create_ack = true;
                    expected_packet = pkt.seq + 1;
                    arr_insert(recv_buffer, pkt.seq);
                }
            }
        }

        // Read from stdin if we can
        if ((int)packets_inflight.size() < MAX_INFLIGHT)
        {
            int bytes_sent = read(STDIN_FILENO, buffer, MAX_PAYLOAD);

            // 3 cases: data + no ack, data + yes ack, no data + yes ack
            if (create_ack == false && bytes_sent > 0)
            {
                int pkt_size = create_packet(buffer, current_packet, expected_packet, 1012, false, bytes_sent);

                sendto(sockfd, buffer, pkt_size, 0, (struct sockaddr *)&addr, sizeof(addr));
                fprintf(stderr, "Sent SEQ: %d, ACK %d\n", current_packet, expected_packet);

                arr_insert(packets_inflight, current_packet); // number of packets unacked

                insert_timer(current_packet, buffer, pkt_size); // set up timer

                current_packet++; // increment the SEQ
            }
            else if (create_ack == true && bytes_sent > 0)
            {
                // get packet in buffer to ACK
                int to_ack = recv_buffer.front();

                // create and send
                int pkt_size = create_packet(buffer, current_packet, expected_packet, 1012, true, bytes_sent);
                sendto(sockfd, buffer, pkt_size, 0, (struct sockaddr *)&addr, sizeof(addr));
                fprintf(stderr, "Sent SEQ: %d, ACK %d\n", current_packet, expected_packet);

                arr_insert(packets_inflight, current_packet); // number of packets unacked

                arr_remove(recv_buffer, to_ack); // remove it from our unacked buffers

                insert_timer(current_packet, buffer, pkt_size); // set up timer

                current_packet++; // increment our SEQ
                expected_packet = to_ack + 1;
                create_ack = false;

                // write out contents of that packet
                // fprintf(stderr, "[DEBUG] SEQ: %d: ", pkt.seq);
                write(STDOUT_FILENO, pkt.payload, pkt.length);
            }
            else if (create_ack == true)
            {
                // get packet in buffer to ACK
                int to_ack = recv_buffer.front();

                // create an ACK and send it out
                int pkt_size = create_packet(buffer, current_packet, to_ack + 1, 1012, true, 0);
                sendto(sockfd, buffer, pkt_size, 0, (struct sockaddr *)&addr, sizeof(addr));
                fprintf(stderr, "Sent SEQ: %d, ACK %d\n", current_packet, expected_packet);

                // remove it from our unacked buffers
                arr_remove(recv_buffer, to_ack);
                current_packet++; // increment our SEQ
                expected_packet = to_ack + 1;
                create_ack = false;

                // write out contents of that packet
                // fprintf(stderr, "[DEBUG] SEQ: %d: ", pkt.seq);
                write(STDOUT_FILENO, pkt.payload, pkt.length);
            }
        }
        else if (create_ack == true) // if we've hit max inflight packets but have an ACK to send out, just do it
        {
            // get packet in buffer to ACK
            int to_ack = recv_buffer.front();

            // create an ACK and send it out
            int pkt_size = create_packet(buffer, current_packet, to_ack + 1, 1012, true, 0);
            sendto(sockfd, buffer, pkt_size, 0, (struct sockaddr *)&addr, sizeof(addr));
            fprintf(stderr, "Sent SEQ: %d, ACK %d\n", current_packet, expected_packet);

            // remove it from our unacked buffers
            arr_remove(recv_buffer, to_ack);
            current_packet++; // increment our SEQ
            expected_packet = to_ack + 1;
            create_ack = false;

            // write out contents of that packet
            write(STDOUT_FILENO, pkt.payload, pkt.length);
        }
    }

    return 0;
}
