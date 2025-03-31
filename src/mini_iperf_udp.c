#include "mini_iperf.h"
#define MAX_PACKET_SIZE 1500  // MTU-safe max size
char packet[MAX_PACKET_SIZE]; // stack buffer
static uint32_t compute_crc32(const uint8_t* data, size_t len);
void* udp_sendto(void* args_ptr) {
    struct arguments* args = (struct arguments*)args_ptr;
    if (args->packet_size > MAX_PACKET_SIZE) {
        fprintf(stderr, "Packet size too large (max %d bytes)\n", MAX_PACKET_SIZE);
        return NULL;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("UDP socket creation failed");
        return NULL;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(args->port + 1);  // UDP port is TCP+1
    server_addr.sin_addr.s_addr = inet_addr(args->ip_address);

    // Calculate delay for throttling (in microseconds)
    double delay_usec = (args->packet_size * 8.0 / args->bandwidth) * 1e6;

    // Create and fill the packet
    uint8_t packet[MAX_PACKET_SIZE];
    MiniIperfHeader* header = (MiniIperfHeader*)packet;
    int payload_size = args->packet_size - sizeof(MiniIperfHeader);
    uint8_t* payload = packet + sizeof(MiniIperfHeader);

    if (args->wait_duration > 0) sleep(args->wait_duration);

    struct timespec start_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);

    uint32_t seq = 0;


    while (1) {
        struct timespec now_ts;
        clock_gettime(CLOCK_MONOTONIC, &now_ts);
        double elapsed_sec = (now_ts.tv_sec - start_ts.tv_sec) +
                             (now_ts.tv_nsec - start_ts.tv_nsec) / 1e9;

        if (args->duration > 0 && elapsed_sec >= args->duration) break;

        // Fill header
        header->type = 0;
        header->flags = args->measure_delay ? 1 : 0;
        header->stream_id = htons(0);
        header->packet_size = htonl(args->packet_size);
        header->bandwidth = htonl(args->bandwidth);
        header->duration = htonl(args->duration);
        header->timestamp_sec = htonl((uint32_t)time(NULL));
        header->sequence_number = htonl(seq);

        // Fill payload
        memset(payload, 'A' + (seq % 26), payload_size);

        // Compute CRC over entire packet except the CRC field
        header->crc32 = 0;
        header->crc32 = htonl(compute_crc32(packet, args->packet_size));

        sendto(sock, packet, args->packet_size, 0,
               (struct sockaddr*)&server_addr, sizeof(server_addr));

        if(args->bandwidth>0) {
            usleep((int)delay_usec);
        }
        seq++;
        if (args->duration > 0 && elapsed_sec >= args->duration) break;
        
    }
    printf("Ended\n");
    close(sock);
    return NULL;
}
void* udp_recv(void* args_ptr) {
    struct arguments* args = (struct arguments*)args_ptr;
    if (args->packet_size > MAX_PACKET_SIZE) {
        fprintf(stderr, "Packet size too large\n");
        return NULL;
    }
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("UDP socket creation failed");
        return NULL;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(args->port + 1);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("UDP bind failed");
        close(sock);
        return NULL;
    }

    uint8_t packet[MAX_PACKET_SIZE];
    uint64_t total_bytes = 0;
    uint64_t payload_bytes = 0;
    uint32_t received_packets = 0;
    uint32_t corrupt_packets = 0;
    uint32_t last_seq = 0xFFFFFFFF;
    struct timespec first_ts = {0}, last_ts = {0};
    struct timeval timeout;
    timeout.tv_sec = 1;  // Set timeout to 1 second
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    while (1) {
        
        ssize_t bytes = recv(sock, packet, args->packet_size, 0);
        if (bytes <= 0) break;

        struct timespec now_ts;
        clock_gettime(CLOCK_MONOTONIC, &now_ts);

        if (received_packets == 0) first_ts = now_ts;
        last_ts = now_ts;

        MiniIperfHeader* header = (MiniIperfHeader*)packet;

        uint32_t received_crc = ntohl(header->crc32);
        header->crc32 = 0;
        uint32_t computed_crc = compute_crc32(packet, args->packet_size);

        if (received_crc != computed_crc) {
            corrupt_packets++;
            continue;
        }

        uint32_t seq = ntohl(header->sequence_number);
        if (seq > last_seq) last_seq = seq;

        total_bytes += bytes;
        payload_bytes += (bytes - sizeof(MiniIperfHeader));
        received_packets++;
        if (args->duration > 0) {
            double elapsed_sec = (now_ts.tv_sec - first_ts.tv_sec) +
                                 (now_ts.tv_nsec - first_ts.tv_nsec) / 1e9;
            if (elapsed_sec >= args->duration) break;
        }
    }
    double duration_sec = (last_ts.tv_sec - first_ts.tv_sec) +(last_ts.tv_nsec - first_ts.tv_nsec) / 1e9;

    double throughput_mbps = (total_bytes * 8.0) / (duration_sec * 1e6);
    double goodput_mbps = (payload_bytes * 8.0) / (duration_sec * 1e6);
    double loss_pct = (last_seq + 1 - received_packets) * 100.0 / (last_seq + 1);
    
    printf("UDP Statistics:\n");
    printf("Total Bytes: %.2f MB\n", total_bytes / 1e6);
    printf("Payload Bytes: %.2f MB\n", payload_bytes / 1e6);
    printf("Received Packets: %u\n", received_packets);
    printf("Corrupt Packets: %u\n", corrupt_packets);
    printf("Lost Packets: %u (%.2f%%)\n", last_seq + 1 - received_packets, loss_pct);
    printf("Throughput: %.2f Mbps\n", throughput_mbps);
    printf("Goodput: %.2f Mbps\n", goodput_mbps);
    printf("Duration: %.2f seconds\n", duration_sec);
    printf("Start Time: %ld.%09ld\n", first_ts.tv_sec, first_ts.tv_nsec);
    printf("End Time: %ld.%09ld\n", last_ts.tv_sec, last_ts.tv_nsec);
    printf("Last Sequence Number: %u\n", last_seq);
    printf("Last Timestamp: %ld.%09ld\n", last_ts.tv_sec, last_ts.tv_nsec);
    close(sock);
    return NULL;
}
static uint32_t compute_crc32(const uint8_t* data, size_t len) {
    uint32_t crc = ~0U;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}
