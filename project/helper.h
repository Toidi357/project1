#include <unistd.h>
#include <vector>
#include <cstring>
#include <arpa/inet.h>

// Struct for our inflight and recv buffers
struct PacketInfo {
    int seq;
    uint8_t packet_data[1024];
    size_t packet_size;
};

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
    if (payload_size > MAX_PAYLOAD)
    {
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

// use create_packet and sends it out too, returns packet send size
int create_and_send(int sockfd, struct sockaddr_in addr, uint8_t *buffer, int seq, int ack, int win, bool ackflag, int payload_size)
{
    int pkt_size = create_packet(buffer, seq, ack, win, ackflag, payload_size);
    sendto(sockfd, buffer, pkt_size, 0, (struct sockaddr *)&addr, sizeof(addr));

    return pkt_size;
}

// These 2 functions help create the sending and receiving buffer
// and sorted in order
static void arr_insert(std::vector<PacketInfo> &arr, const PacketInfo &element) {
    auto pos = std::lower_bound(arr.begin(), arr.end(), element,
                                [](const PacketInfo &a, const PacketInfo &b) {
                                    return a.seq < b.seq;
                                });
    arr.insert(pos, element);
}
static void arr_remove(std::vector<PacketInfo> &arr, int seq) {
    auto pos = std::lower_bound(arr.begin(), arr.end(), seq,
                                [](const PacketInfo &a, int seq) {
                                    return a.seq < seq;
                                });
    if (pos != arr.end() && pos->seq == seq) {
        arr.erase(arr.begin(), pos + 1);
    }
}

static void arr_insert_std(std::vector<int> &arr, int element)
{
    // Find the correct position to insert the element
    auto pos = std::lower_bound(arr.begin(), arr.end(), element);
    // Insert the element at the correct position
    arr.insert(pos, element);
}
static void arr_remove_std(std::vector<int> &arr, int element)
{
    auto pos = std::lower_bound(arr.begin(), arr.end(), element);
    if (pos != arr.end() && *pos == element)
    {
        arr.erase(arr.begin(), pos + 1);
    }
}

// Data structure to determine whether 3 dup ACKs have been received or not
class DupACKs {
public:
    bool add(int ack) { // returns 1 if after adding, its 3 dup acks
        // sliding window logic
        arr[0] = arr[1];
        arr[1] = arr[2];
        arr[2] = ack;

        if (arr[0] == arr[1] and arr[1] == arr[2]) {
            dupAcks = true;
        }
        else {
            dupAcks = false;
        }
        return dupAcks;
    };
    bool dupAcks = false;
private:
    std::vector<int> arr = {-1, -1, -1};
};